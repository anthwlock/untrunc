/*
	Reference-driven MXF repair

	This is a deliberately narrow repair path for MXF recordings whose header,
	footer, index, or Random Index Pack (RIP) is missing or unreliable while the
	essence remains readable. It needs a known-good recording from the same
	recorder profile. The reference contributes only structural metadata; it
	never contributes media packets to the repaired result.

	Usage:
		untrunc -mxf reference.mxf damaged.mxf
		untrunc -mxf -dst repaired.mxf reference.mxf damaged.mxf

	The workflow is:
	  1. Scan both files in fixed-size windows using 64-bit offsets. Files are
	     never loaded wholesale into memory.
	  2. Find and bounds-check Header, Body, and Footer Partition Pack KLVs, and
	     locate a Random Index Pack when present.
	  3. Copy the reference header, then the damaged body/footer, into a temporary
	     structural file. Rebase ThisPartition, PreviousPartition, and
	     FooterPartition after the header-size change; discard a stale RIP.
	  4. Let libavformat demux the temporary file and compare its complete stream
	     profile with the reference. Then remux recovered packets through the
	     appropriate FFmpeg MXF muxer, which recreates index tables, durations,
	     KAG layout, partition offsets, footer metadata, and the RIP.

	Repair fails without modifying either input when the required partition packs
	are missing or out of bounds; KLV BER lengths are invalid; Operational
	Patterns, SIDs, or stream profiles disagree; no packets can be recovered; or
	libavformat cannot read or write the necessary MXF representation. There is
	intentionally no force option that bypasses compatibility checks.

	Regression coverage:
	  - a destroyed header is replaced from a compatible reference;
	  - damaged-file video and audio packets are recovered and reindexed;
	  - ffprobe can read the expected codecs and recovered packet count;
	  - the output is checked not to substitute the reference recording;
	  - temporary structural files are removed;
	  - Operational Pattern and raster-profile mismatches are rejected.

	Format references used to implement and cross-check this code:
	  Published specifications
	    - SMPTE ST 377-1:2019, Material Exchange Format (MXF) -- File Format
	      Specification: https://pub.smpte.org/latest/st377-1/st377-1-2019.pdf
	    - SMPTE ST 336:2017, Data Encoding Protocol Using Key-Length-Value:
	      https://pub.smpte.org/pub/st336/st0336-2017.pdf
	  Open implementations
	    - FFmpeg MXF demuxer and muxer:
	      https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/mxfdec.c
	      https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/mxfenc.c
	    - HVLC's libavformat-based MXF integration, used as a practical check that
	      libavformat remains the MXF demuxing boundary in a broadcast workflow:
	      https://github.com/htv-broadcast/HVLC/blob/master/modules/demux/avformat/demux.c

	The repair procedure itself is Untrunc-specific and was inferred from the
	partition-pack layout in the published specifications, from FFmpeg-generated
	fixtures, and from damaged recordings. It is not a complete MXF parser or a
	vendor-specific format description. In particular, copying the known-good
	header, rebasing the damaged partitions, and remuxing recoverable packets is
	a conservative recovery strategy rather than a normative MXF operation.
*/
#include "mxf.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
}

using namespace std;

namespace {
// SMPTE ST 377-1 Partition Pack UL prefix. Byte 13 identifies Header, Body,
// or Footer Partition Packs; the remaining bytes are deliberately not matched.
constexpr array<uint8_t, 13> kPartitionPrefix = {
	0x06, 0x0e, 0x2b, 0x34, 0x02, 0x05, 0x01, 0x01, 0x0d, 0x01, 0x02, 0x01, 0x01};
// SMPTE ST 377-1 Random Index Pack UL. A RIP belongs to the old file layout,
// so it must not be copied into an intermediate with rebased partitions.
constexpr array<uint8_t, 16> kRipKey = {
	0x06, 0x0e, 0x2b, 0x34, 0x02, 0x05, 0x01, 0x01, 0x0d, 0x01, 0x02, 0x01, 0x01, 0x11, 0x01, 0x00};
constexpr size_t kWindow = 4 * 1024 * 1024;

struct Partition {
	uint64_t offset = 0, value_offset = 0, value_length = 0;
	uint8_t kind = 0;
	uint32_t index_sid = 0, body_sid = 0;
	array<uint8_t, 16> operational_pattern{};
};

uint32_t be32(const uint8_t* p) {
	return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
void put32(ostream& out, uint32_t v) {
	uint8_t b[4] = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
	out.write(reinterpret_cast<char*>(b), 4);
}
void put64(ostream& out, uint64_t v) {
	uint8_t b[8];
	for (int i = 7; i >= 0; --i) {
		b[i] = uint8_t(v);
		v >>= 8;
	}
	out.write(reinterpret_cast<char*>(b), 8);
}
void patch64(fstream& out, uint64_t off, uint64_t v) {
	out.seekp(static_cast<streamoff>(off));
	put64(out, v);
	if (!out)
		throw runtime_error("could not patch MXF partition");
}
uint64_t fileSize(const string& path) {
	ifstream f(path, ios::binary | ios::ate);
	if (!f)
		throw runtime_error("could not open MXF file: " + path);
	auto n = f.tellg();
	if (n < 0)
		throw runtime_error("could not size MXF file: " + path);
	return static_cast<uint64_t>(n);
}
bool readAt(ifstream& f, uint64_t off, uint8_t* p, size_t n) {
	f.clear();
	f.seekg(static_cast<streamoff>(off));
	return bool(f.read(reinterpret_cast<char*>(p), static_cast<streamsize>(n)));
}
// KLV lengths use short BER or a one-byte count followed by up to eight
// big-endian bytes. Indefinite and over-wide forms are refused before offset
// arithmetic.
bool readBer(ifstream& f, uint64_t off, uint64_t& value, size_t& bytes) {
	uint8_t first;
	if (!readAt(f, off, &first, 1))
		return false;
	if (!(first & 0x80)) {
		value = first;
		bytes = 1;
		return true;
	}
	size_t count = first & 0x7f;
	if (!count || count > 8)
		return false;
	uint8_t b[8];
	if (!readAt(f, off + 1, b, count))
		return false;
	value = 0;
	for (size_t i = 0; i < count; ++i)
		value = (value << 8) | b[i];
	bytes = 1 + count;
	return true;
}
bool parsePartition(ifstream& f, uint64_t size, uint64_t off, Partition& p) {
	uint8_t key[16];
	if (!readAt(f, off, key, 16) || !equal(kPartitionPrefix.begin(), kPartitionPrefix.end(), key))
		return false;
	if (key[13] < 2 || key[13] > 4)
		return false;
	uint64_t len;
	size_t ber;
	if (!readBer(f, off + 16, len, ber) || len < 80 || off + 16 + ber > size || len > size - (off + 16 + ber))
		return false;
	// The first 80 Partition Pack bytes hold fixed fields. ThisPartition is at
	// +8, PreviousPartition at +16, FooterPartition at +24, IndexSID at +48,
	// BodySID at +60, and the Operational Pattern UL at +64.
	uint8_t head[80];
	uint64_t value = off + 16 + ber;
	if (!readAt(f, value, head, sizeof(head)))
		return false;
	p.offset = off;
	p.kind = key[13];
	p.value_offset = value;
	p.value_length = len;
	p.index_sid = be32(head + 48);
	p.body_sid = be32(head + 60);
	copy(head + 64, head + 80, p.operational_pattern.begin());
	return true;
}
struct Scan {
	uint64_t size = 0, rip = 0;
	vector<Partition> parts;
};
Scan scan(const string& path) {
	ifstream f(path, ios::binary);
	if (!f)
		throw runtime_error("could not open MXF file: " + path);
	Scan s;
	s.size = fileSize(path);
	s.rip = s.size;
	// Keep 15 bytes from each window so a 16-byte UL crossing a boundary is
	// found.
	vector<uint8_t> buf(kWindow + 15);
	uint64_t base = 0;
	size_t carry = 0;
	while (base < s.size) {
		uint64_t want = min<uint64_t>(kWindow, s.size - base);
		f.clear();
		f.seekg(static_cast<streamoff>(base));
		f.read(reinterpret_cast<char*>(buf.data() + carry), static_cast<streamsize>(want));
		size_t got = carry + static_cast<size_t>(f.gcount());
		uint64_t start = base - carry;
		for (size_t i = 0; i + 16 <= got; ++i) {
			uint64_t absolute = start + i;
			if (equal(kRipKey.begin(), kRipKey.end(), buf.begin() + i))
				s.rip = min(s.rip, absolute);
			if (equal(kPartitionPrefix.begin(), kPartitionPrefix.end(), buf.begin() + i)) {
				Partition p;
				if (parsePartition(f, s.size, absolute, p) && (s.parts.empty() || s.parts.back().offset != p.offset))
					s.parts.push_back(p);
			}
		}
		carry = min<size_t>(15, got);
		if (carry)
			copy(buf.begin() + got - carry, buf.begin() + got, buf.begin());
		base += want;
	}
	return s;
}
const Partition& first(const vector<Partition>& ps, uint8_t kind, const char* label) {
	for (const auto& p : ps)
		if (p.kind == kind)
			return p;
	throw runtime_error(string("MXF has no valid ") + label + " partition");
}
void copyRange(ifstream& in, ostream& out, uint64_t begin, uint64_t end) {
	vector<char> b(kWindow);
	in.clear();
	in.seekg(static_cast<streamoff>(begin));
	while (begin < end) {
		size_t n = static_cast<size_t>(min<uint64_t>(b.size(), end - begin));
		if (!in.read(b.data(), static_cast<streamsize>(n)))
			throw runtime_error("truncated MXF while copying");
		out.write(b.data(), static_cast<streamsize>(n));
		if (!out)
			throw runtime_error("could not write MXF intermediate");
		begin += n;
	}
}
string avError(int code) {
	char b[AV_ERROR_MAX_STRING_SIZE];
	av_strerror(code, b, sizeof(b));
	return b;
}
void validateProfiles(const string& reference, const string& reconstructed) {
	AVFormatContext* a = nullptr;
	AVFormatContext* b = nullptr;
	auto openInfo = [](const string& path, AVFormatContext** ctx) {
		int r = avformat_open_input(ctx, path.c_str(), nullptr, nullptr);
		if (r >= 0)
			r = avformat_find_stream_info(*ctx, nullptr);
		return r;
	};
	int r = openInfo(reference, &a);
	if (r < 0)
		throw runtime_error("could not inspect reference MXF profile: " + avError(r));
	r = openInfo(reconstructed, &b);
	if (r < 0) {
		avformat_close_input(&a);
		throw runtime_error("could not inspect reconstructed MXF profile: " + avError(r));
	}
	// A matching Partition Pack alone is insufficient: the rebuilt structure must
	// still describe the same streams as the known-good recorder profile.
	string mismatch;
	if (a->nb_streams != b->nb_streams)
		mismatch = "stream count";
	for (unsigned i = 0; mismatch.empty() && i < a->nb_streams; ++i) {
		const AVStream* x = a->streams[i];
		const AVStream* y = b->streams[i];
		const AVCodecParameters* p = x->codecpar;
		const AVCodecParameters* q = y->codecpar;
		if (p->codec_type != q->codec_type || p->codec_id != q->codec_id)
			mismatch = "codec";
		else if (p->width != q->width || p->height != q->height)
			mismatch = "video dimensions";
		else if (p->sample_rate != q->sample_rate)
			mismatch = "audio sample rate";
		else if (p->format != q->format && p->format >= 0 && q->format >= 0)
			mismatch = "media sample format";
		else if (av_cmp_q(x->time_base, y->time_base))
			mismatch = "stream time base";
		else if (p->extradata_size && q->extradata_size &&
				 (p->extradata_size != q->extradata_size ||
					 !equal(p->extradata, p->extradata + p->extradata_size, q->extradata)))
			mismatch = "codec configuration";
	}
	avformat_close_input(&a);
	avformat_close_input(&b);
	if (!mismatch.empty())
		throw runtime_error("reference and damaged MXF profiles differ: " + mismatch);
}
void remux(const string& input, const string& output, const char* muxer) {
	AVFormatContext* in = nullptr;
	AVFormatContext* out = nullptr;
	AVPacket pkt;
	int r = avformat_open_input(&in, input.c_str(), nullptr, nullptr);
	if (r < 0)
		throw runtime_error("reconstructed MXF could not be opened: " + avError(r));
	auto closeIn = [&]() {
		if (in)
			avformat_close_input(&in);
	};
	r = avformat_find_stream_info(in, nullptr);
	if (r < 0) {
		closeIn();
		throw runtime_error("could not read reconstructed MXF streams: " + avError(r));
	}
	// Remuxing from packets regenerates MXF indexes and footer metadata instead
	// of retaining damaged index/footer data.
	r = avformat_alloc_output_context2(&out, nullptr, muxer, output.c_str());
	if (r < 0 || !out) {
		closeIn();
		throw runtime_error("FFmpeg MXF muxer is unavailable");
	}
	av_dict_copy(&out->metadata, in->metadata, 0);
	for (unsigned i = 0; i < in->nb_streams; ++i) {
		AVStream* src = in->streams[i];
		AVStream* dst = avformat_new_stream(out, nullptr);
		if (!dst) {
			avformat_free_context(out);
			closeIn();
			throw runtime_error("could not allocate MXF stream");
		}
		r = avcodec_parameters_copy(dst->codecpar, src->codecpar);
		if (r < 0) {
			avformat_free_context(out);
			closeIn();
			throw runtime_error("could not copy MXF stream parameters");
		}
		dst->codecpar->codec_tag = 0;
		dst->time_base = src->time_base;
		av_dict_copy(&dst->metadata, src->metadata, 0);
	}
	if (!(out->oformat->flags & AVFMT_NOFILE)) {
		r = avio_open(&out->pb, output.c_str(), AVIO_FLAG_WRITE);
		if (r < 0) {
			avformat_free_context(out);
			closeIn();
			throw runtime_error("could not create repaired MXF: " + avError(r));
		}
	}
	r = avformat_write_header(out, nullptr);
	if (r < 0) {
		if (out->pb)
			avio_closep(&out->pb);
		avformat_free_context(out);
		closeIn();
		throw runtime_error("could not write MXF header: " + avError(r));
	}
	int64_t packets = 0;
	while ((r = av_read_frame(in, &pkt)) >= 0) {
		AVStream* src = in->streams[pkt.stream_index];
		AVStream* dst = out->streams[pkt.stream_index];
		av_packet_rescale_ts(&pkt, src->time_base, dst->time_base);
		pkt.pos = -1;
		r = av_interleaved_write_frame(out, &pkt);
		av_packet_unref(&pkt);
		if (r < 0)
			break;
		++packets;
	}
	if (r != AVERROR_EOF && r < 0) {
		avio_closep(&out->pb);
		avformat_free_context(out);
		closeIn();
		throw runtime_error("failed while rebuilding MXF edit units: " + avError(r));
	}
	r = av_write_trailer(out);
	if (out->pb)
		avio_closep(&out->pb);
	avformat_free_context(out);
	closeIn();
	if (r < 0 || !packets)
		throw runtime_error(
			!packets ? "no recoverable MXF edit units" : "could not finalize repaired MXF: " + avError(r));
	cout << "Reindexed " << packets << " recovered MXF packet(s)" << endl;
}
} // namespace

void MxfRepair::repair(const string& reference, const string& damaged, const string& destination) {
	Scan gs = scan(reference), ds = scan(damaged);
	const auto& gh = first(gs.parts, 2, "reference header");
	const auto& gb = first(gs.parts, 3, "reference body");
	const auto& db = first(ds.parts, 3, "damaged body");
	if (gh.operational_pattern != db.operational_pattern)
		throw runtime_error("reference and damaged MXF Operational Patterns differ");
	if (gb.body_sid != db.body_sid || gb.index_sid != db.index_sid)
		throw runtime_error("reference and damaged MXF BodySID/IndexSID differ");
	uint64_t bad_end = ds.rip;
	if (bad_end < db.offset)
		throw runtime_error("damaged MXF RIP precedes its body");
	string target = destination.empty() ? damaged + "_fixed.mxf" : destination;
	string intermediate = target + ".untrunc-structure.tmp";
	{
		ifstream good(reference, ios::binary), bad(damaged, ios::binary);
		fstream out(intermediate, ios::binary | ios::in | ios::out | ios::trunc);
		if (!good || !bad || !out)
			throw runtime_error("could not create MXF repair intermediate");
		// Copy only reference metadata; damaged-file body/footer data supplies
		// every packet.
		copyRange(good, out, 0, gb.offset);
		copyRange(bad, out, db.offset, bad_end);
		int64_t delta = int64_t(gb.offset) - int64_t(db.offset);
		uint64_t footer = 0;
		vector<Partition> copied;
		for (const auto& p : ds.parts)
			if (p.offset >= db.offset && p.offset < bad_end) {
				copied.push_back(p);
				if (p.kind == 4)
					footer = uint64_t(int64_t(p.offset) + delta);
			}
		// Header replacement changes offsets, so rebase each copied Partition Pack.
		patch64(out, gh.value_offset + 24, footer);
		uint64_t previous = 0;
		for (const auto& p : copied) {
			uint64_t here = uint64_t(int64_t(p.offset) + delta);
			uint64_t value = uint64_t(int64_t(p.value_offset) + delta);
			patch64(out, value + 8, here);
			patch64(out, value + 16, previous);
			patch64(out, value + 24, footer);
			previous = here;
		}
	}
	// Preserve the existing OP-Atom selection encoded in the Operational Pattern
	// UL.
	const char* muxer = gh.operational_pattern[13] == 0x0d ? "mxf_opatom" : "mxf";
	try {
		validateProfiles(reference, intermediate);
		remux(intermediate, target, muxer);
	} catch (...) {
		remove(intermediate.c_str());
		remove(target.c_str());
		throw;
	}
	remove(intermediate.c_str());
	cout << "Recovered MXF using compatible reference metadata: " << target << endl;
}
