#include "mp4.h"
#include "file.h"
#include "hvc1/picture_order.h"

#include <cstring>
#include <limits>

using namespace std;

namespace {

uint32_t readLe32(const uchar* p) {
	return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
	       uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

struct CanonHeader {
	string type;
	uint32_t length = 0, sample_size = 0, n_samples = 0;
	vector<uint32_t> sizes;

	uint32_t sizeAt(uint32_t i) const {
		return sample_size ? sample_size : sizes[i];
	}
};

// CAEP recordings can store a little-endian recovery index before each group
// of samples: "repi", 24-bit header length, 0x83, type, fixed size, sample count,
// then one size per sample when the fixed size is zero.
bool readCanonHeader(BufferedAtom& mdat, off_t off, CanonHeader& header) {
	if (off < 0 || mdat.contentSize() - off < 20) return false;
	auto p = mdat.getFragment(off, 20);
	if (memcmp(p, "repi", 4) || p[7] != 0x83) return false;

	CanonHeader h;
	h.type = string(reinterpret_cast<const char*>(p + 8), 4);
	if (!contains({"vide", "aac ", "tmcd"}, h.type)) return false;
	h.length = readLe32(p + 4) & 0xffffff;
	h.sample_size = readLe32(p + 12);
	h.n_samples = readLe32(p + 16);
	if (!h.n_samples || h.sample_size > uint32_t(numeric_limits<int>::max())) return false;
	uint64_t expected = 20 + (h.sample_size ? 0 : uint64_t(h.n_samples) * 4);
	if (h.length != expected || h.length > mdat.contentSize() - off) return false;
	// Even an incomplete payload must contain at least one complete sample.
	if (h.sample_size && h.sample_size > mdat.contentSize() - off - h.length) return false;

	if (!h.sample_size) {
		h.sizes.reserve(h.n_samples);
		for (uint32_t i = 0; i < h.n_samples; ++i) {
			auto size = readLe32(mdat.getFragment(off + 20 + uint64_t(i) * 4, 4));
			if (!size || size > uint32_t(numeric_limits<int>::max())) return false;
			h.sizes.push_back(size);
		}
	}
	header = move(h);
	return true;
}

int canonTrack(const vector<Track>& tracks, const string& type) {
	int found = -1;
	for (size_t i = 0; i < tracks.size(); ++i) {
		auto& t = tracks[i];
		bool matches = (type == "vide" && contains({"hvc1", "hev1"}, t.codec_.name_)) ||
		               (type == "aac " && t.codec_.name_ == "mp4a") ||
		               (type == "tmcd" && t.codec_.name_ == "tmcd");
		if (!matches) continue;
		if (found >= 0) return -1;  // The index does not identify duplicate tracks.
		found = i;
	}
	return found;
}

// Search only between indexed payloads. A bare "repi" inside compressed data
// is insufficient: the complete header, type and size table must also validate.
off_t findCanonHeader(BufferedAtom& mdat, off_t off, CanonHeader& header) {
	const int window = 1 << 16;
	while (mdat.contentSize() - off >= 20) {
		int size = min<int64_t>(window, mdat.contentSize() - off);
		auto p = mdat.getFragment(off, size);
		int candidate = -1;
		for (int i = 0; i <= size - 20; ++i) {
			if (!memcmp(p + i, "repi", 4) && p[i + 7] == 0x83) {
				candidate = i;
				break;
			}
		}
		if (candidate >= 0) {
			off += candidate;
			if (readCanonHeader(mdat, off, header)) return off;
			++off;  // Reload the search buffer after reading a rejected header.
		}
		else off += size - 19;
	}
	return -1;
}

} // namespace

bool Mp4::repairCanon() {
	CanonHeader header;
	auto& mdat = *current_mdat_;
	if (!has_canon_brand_ || !readCanonHeader(mdat, 0, header) ||
	    canonTrack(tracks_, header.type) < 0) return false;
	// Other Canon variants use PCM audio or different index layouts. Keep
	// those recordings on the existing scanner rather than skipping a track.
	int video_idx = canonTrack(tracks_, "vide");
	if (video_idx < 0) return false;
	int audio_idx = canonTrack(tracks_, "aac ");
	int timecode_idx = canonTrack(tracks_, "tmcd");
	for (size_t i = 0; i < tracks_.size(); ++i) {
		if (tracks_[i].is_dummy_) continue;
		if (int(i) != video_idx && int(i) != audio_idx && int(i) != timecode_idx) return false;
	}
	HevcPictureOrder parser(tracks_[video_idx].codec_.av_codec_params_);

	logg(I, "using Canon repi recovery indexes\n");
	off_t offset = 0;
	while (offset < mdat.contentSize()) {
		auto header_off = findCanonHeader(mdat, offset, header);
		if (header_off < 0) {
			addToExclude(offset, mdat.contentSize() - offset);
			break;
		}
		int idx = canonTrack(tracks_, header.type);
		if (idx < 0) throw string("Canon recovery index has no unique matching reference track");
		auto& track = tracks_[idx];
		if (header.sample_size && track.constant_size_ != int(header.sample_size))
			throw string("Canon recovery index sample size does not match the reference track");

		auto payload = header_off + header.length;
		addToExclude(offset, payload - offset);
		offset = payload;
		logg(V, "Canon ", header.type, " index at ", offToStr(header_off),
		     ": ", header.n_samples, " samples\n");

		for (uint32_t i = 0; i < header.n_samples; ++i) {
			uint32_t size = header.sizeAt(i);
			if (track.constant_size_ && size != uint32_t(track.constant_size_))
				throw string("Canon recovery index sample size does not match the reference track");
			if (size > mdat.contentSize() - offset) {
				logg(W, "incomplete Canon sample at ", offToStr(offset), "\n");
				addToExclude(offset, mdat.contentSize() - offset);
				offset = mdat.contentSize();
				break;
			}
			bool keyframe = false;
			if (header.type == "vide") {
				if (size < 7) throw string("Canon video sample is too short");
				auto picture = parser.parse(mdat, offset, size);
				keyframe = picture.keyframe;
			}

			FrameInfo frame(idx, keyframe, 0, offset, size);
			if (use_offset_map_ && !track.constant_size_) chkFrameDetectionAt(frame, offset);
			Track::Chunk chunk(offset, size, 1);
			chunk.already_excluded_ = mdat.total_excluded_yet_;
			track.chunks_.push_back(chunk);
			addFrame(frame);
			++pkt_idx_;
			offset += size;

			int64_t padding = (track.pkt_sz_gcd_ - size % track.pkt_sz_gcd_) % track.pkt_sz_gcd_;
			padding = min<int64_t>(padding, mdat.contentSize() - offset);
			if (padding) addToExclude(offset, padding);
			offset += padding;
		}
	}
	auto& video = tracks_[video_idx];
	video.fixTimes();
	parser.apply(video);
	return true;
}
