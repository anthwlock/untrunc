#include "codec.h"

#include <iostream>
#include <vector>
#include <cassert>

extern "C" {
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

#include "common.h"
#include "atom.h"
#include "avc1/avc1.h"
#include "avc1/avc-config.h"
#include "hvc1/hvc1.h"
#include "mp4.h"

using namespace std;

extern map<string, bool(*) (Codec*, const uchar*, uint)> dispatch_match;
extern map<string, bool(*) (Codec*, const uchar*, uint)> dispatch_strict_match;
extern map<string, int(*) (Codec*, const uchar*, uint)> dispatch_get_size;

bool Codec::twos_is_sowt = false;
Codec::Codec(AVCodecParameters* c) : av_codec_params_(c) {}

void Codec::initAVCodec() {
	auto c = av_codec_params_;
	auto av_codec = avcodec_find_decoder(c->codec_id);

	if (!av_codec) {
		auto codec_type = av_get_media_type_string(c->codec_type);
		auto codec_name = avcodec_get_name(c->codec_id);
		logg(V, "FFmpeg does not support codec: <", codec_type, ", ", codec_name, ">\n");
		return;
	}

	av_codec_context_ = avcodec_alloc_context3(av_codec);
	avcodec_parameters_to_context(av_codec_context_, c);

	if (avcodec_open2(av_codec_context_, av_codec, NULL) < 0)
		throw "Could not open codec: ?";
}

void Codec::initOnce() {
	static bool did_once = false;
	if (did_once) return;
	did_once = true;

	assert(dispatch_get_size.count("mp4a"));
	dispatch_get_size["sawb"] = dispatch_get_size["mp4a"];
	map<string, vector<string>> alias = {
	    {"hvc1", {"hev1"}},
	    {"ap4x", {"apch", "apcn", "apcs", "apco", "ap4h"}},  // Apple ProRes
	};
	for (auto& p : alias) {
		for (auto& alias : p.second) {
			dispatch_get_size[alias] = dispatch_get_size[p.first];
			dispatch_match[alias] = dispatch_match[p.first];
		}
	}
}


void Codec::parseOk(Atom *trak) {
	Atom *stsd = trak->atomByName("stsd");
	int entries = stsd->readInt(4);
	if(entries != 1)
		throw "Multiplexed stream! Not supported";

	name_ = stsd->getString(12, 4);
	name_ = name_.c_str();  // might be smaller than 4

	if (contains({"mp4a", "sawb", "mp4v"}, name_)) initAVCodec();

	match_fn_ = dispatch_match[name_];
	match_strict_fn_ = dispatch_strict_match[name_];
	get_size_fn_ = dispatch_get_size[name_];

	if (name_ == "avc1") {
		avc_config_ = new AvcConfig(stsd);
		if (!avc_config_->is_ok)
			logg(W, "avcC was not decoded correctly\n");
		else
			logg(V, "avcC got decoded\n");
	}
	else if (name_ == "sowt")
		Codec::twos_is_sowt = true;
}

bool Codec::isSupported() {
	return match_fn_ && get_size_fn_;
}


#define MATCH_FN(codec)  {codec, [](Codec* self __attribute__((unused)), \
	const uchar* start __attribute__((unused)), uint s __attribute__((unused))) -> bool

map<string, bool(*) (Codec*, const uchar*, uint)> dispatch_strict_match {
	MATCH_FN("avc1") {
		int s2 = swap32(((int *)start)[1]);
		if (self->strictness_lvl_ > 0) {
			return s == 0x00000002 && (s2 == 0x09300000 || s2 == 0x09100000);
		}
		int s1 = s;
//		return s1 == 0x01 || s1 == 0x02 || s1 == 0x03;
		if (s1 == 0x01 || (s1>>8) == 0x01 || s1 == 0x02 || s1 == 0x03) return true;

		// vlc 2.1.5 stream output
		if (s1 >> 16 == 0 && s2 >> 16 == 0x619a) return true;
		if (s1 == 0x00000017 && s2 == 0x674d0020) return true;
		return false;
	}},
    MATCH_FN("hvc1") {
		if (start[0] != 0x00 || start[5] != 0x01) return false;
		if (start[4] != 0x02 && start[4] != 0x26 && start[4] != 0x00) return false;
		return true;
	}},
	MATCH_FN("fdsc") {
		if (string((char*)start, 2) != "GP") return false;
		if (start[8] || start[9]) return false;
		return true;
	}},
	MATCH_FN("mp4a") {
		return false;
//		return (s>>16) == 0x210A;  // this needs to be improved
	}},
	MATCH_FN("tmcd") { return false; }},
	MATCH_FN("mebx") { return false; }},
};

// only match if we are certain -> less false positives
// you might want to tweak these values..
bool Codec::matchSampleStrict(const uchar *start) {
	int s = swap32(*(int *)start);  // big endian
	if (!match_strict_fn_) return matchSample(start);
	return match_strict_fn_(this, start, s);
}

bool Codec::looksLikeTwosOrSowt(const uchar* start) {
	if (Codec::twos_is_sowt) start += 1;
	int
	    d1 = abs(start[4]  - start[2]),
	    d2 = abs(start[6]  - start[4]),
	    d3 = abs(start[8]  - start[6]),
	    d4 = abs(start[10]  - start[8]),
	    d5 = abs(start[12]  - start[10]);
	int cnt =
	    (8 < d1 && d1 < 0xf0) +
	    (8 < d2 && d2 < 0xf0) +
	    (8 < d3 && d3 < 0xf0) +
	    (8 < d4 && d4 < 0xf0) +
	    (8 < d5 && d5 < 0xf0);
//	if (cnt <= 1) {
	if (cnt == 0) {
		if (g_log_mode >= LogMode::V) {
			if (Codec::twos_is_sowt) start -= 1;
			printBuffer(start, 16);
			cout << "avc1: detected sowt..\n";
		}
		return true;
	}
	return false;
}

map<string, bool(*) (Codec*, const uchar*, uint)> dispatch_match {
	MATCH_FN("avc1") {
		//this works only for a very specific kind of video
		//#define SPECIAL_VIDEO
#ifdef SPECIAL_VIDEO
		int s2 = swap32(((int *)start)[1]);
		return (s != 0x00000002 || (s2 != 0x09300000 && s2 != 0x09100000)) return false;
		return true;
#endif

		if (self->chk_for_twos_ && Codec::looksLikeTwosOrSowt(start)) return false;

		//TODO use the first byte of the nal: forbidden bit and type!
		int nal_type = (start[4] & 0x1f);
		//the other values are really uncommon on cameras...
		if(nal_type > 21
		   && nal_type != 31  // sometimes used for metadata
		   ) {
			//		if(nal_type != 1 && nal_type != 5 && nal_type != 6 && nal_type != 7 &&
			//				nal_type != 8 && nal_type != 9 && nal_type != 10 && nal_type != 11 && nal_type != 12) {
			logg(V, "avc1: no match because of nal type: ", nal_type, '\n');
			return false;
		}
		//if nal is equal 7, the other fragments (starting with nal type 7)
		//should be part of the same packet
		//(we cannot recover time information, remember)
		if(start[0] == 0) {
			logg(V, "avc1: Match with 0 header\n");
			return true;
		}
		logg(V, "avc1: failed for no particular reason\n");
		return false;
	}},

    MATCH_FN("mp4a") {
		if(s > 1000000) {
			logg(V, "mp4a: Success because of large s value\n");
			return true;
		}
		//horrible hack... these values might need to be changed depending on the file
		if((start[4] == 0xee && start[5] == 0x1b) ||
		   (start[4] == 0x3e && start[5] == 0x64)) {
			logg(W, "mp4a: Success because of horrible hack.\n");
			return true;
		}

		if(start[0] == 0) {
			logg(V, "Failure because of NULL header\n");
			return false;
		}
		logg(V, "Success for no particular reason....\n");
		return true;

		/* THIS is true for mp3...
		//from http://www.mp3-tech.org/ programmers corner
		//first 11 bits as 1,
		//       2 bits as version (mpeg 1, 2 etc)
		//       2 bits layer (I, II, III)
		//       1 bit crc protection
		//       4 bits bitrate (could be variable if VBR)
		//       2 bits sample rate
		//       1 bit padding (might be variable)
		//       1 bit privete (???)
		//       2 bit channel mode (constant)
		//       2 bit mode extension
		//       1 bit copyright
		//       1 bit original
		//       2 bit enfasys.
		//in practice we have
		// 11111111111 11 11 1 0000 11 0 0 11 11 1 1 11 mask
		// 1111 1111 1111 1111 0000 1100 1111 1111 or 0xFFFF0CFF
		reverse(s);
		if(s & 0xFFE00000 != 0xFFE0000)
			return false;
		return true; */
	}},

	MATCH_FN("mp4v") { //as far as I know keyframes are 1b3, frames are 1b6 (ISO/IEC 14496-2, 6.3.4 6.3.5)
		return s == 0x1b3 || s == 0x1b6;
	}},

	MATCH_FN("alac") {
		int t = swap32(*(int *)(start + 4));
		t &= 0xffff0000;

		if(s == 0 && t == 0x00130000) return true;
		if(s == 0x1000 && t == 0x001a0000) return true;
		return false;
	}},

	MATCH_FN("samr") {
		return start[0] == 0x3c;
	}},
	MATCH_FN("apcn") {
		return memcmp(start, "icpf", 4) == 0;
	}},
	MATCH_FN("sawb") {
		return start[0] == 0x44;
	}},
	MATCH_FN("tmcd") {  // GoPro timecode .. hardcoded in Mp4::GetMatches
		return false;
//		return !tmcd_seen_ && start[0] == 0 && g_mp4->wouldMatch(start+4, "tmcd");
	}},
	MATCH_FN("gpmd") {  // GoPro timecode, 4 bytes (?)
		vector<string> fourcc = {"DEVC", "DVID", "DVNM", "STRM", "STNM", "RMRK",  "SCAL",
		                         "SIUN", "UNIT", "TYPE", "TSMP", "TIMO", "EMPT"};
		return contains(fourcc, string((char*)start, 4));
	}},
	MATCH_FN("fdsc") {  // GoPro recovery.. anyone knows more?
		return string((char*)start, 2) == "GP";
	}},
	MATCH_FN("hvc1") {
		// no idea if this generalizes well..
		// 00...... ..01....
		return start[0] == 0x00 && start[5] == 0x01;
	}},
	MATCH_FN("mebx") {
//		return s == 8 || s == 10 || s == 100;
		return s < 200;
	}},
	MATCH_FN("icod") {
		// 0116....
		return start[0] == 1 && start[1] == 22;
	}},
	MATCH_FN("ap4x") {
		return string((char*)start+4, 4) == "icpf";
	}},
	MATCH_FN("camm") {
		return (start[0] == 0 && start[1] == 0) || (start[3] == 0 && start[2] < 7);
	}},
	MATCH_FN("jpeg") {
		return s >> 16 == 0xFFD8;
	}},

	/*
	MATCH_FN("twos") {
		//weird audio codec: each packet is 2 signed 16b integers.
		cerr << "This audio codec is EVIL, there is no hope to guess it.\n";
		return true;
	}},
	MATCH_FN("in24") { //it's a codec id, in a case I found a pcm_s24le (little endian 24 bit) No way to know it's length.
		return true;
	}},
	MATCH_FN("sowt") {
		cerr << "Sowt is just  raw data, no way to guess length (unless reliably detecting the other codec start)\n";
		return false;
	}},
	MATCH_FN("lpcm") {
		// This is not trivial to detect because it is just
		// the audio waveform encoded as signed 16-bit integers.
		// For now, just test that it is not "apcn" video:
		return memcmp(start, "icpf", 4) != 0;
	}},
	*/
};

bool Codec::matchSample(const uchar *start) {
	if (match_fn_) {
		int s = swap32(*(int *)start);  // big endian
		return match_fn_(this, start, s);
	}
	return false;
}


#define GET_SZ_FN(codec)  {codec, [](Codec* self __attribute__((unused)), \
	const uchar* start __attribute__((unused)), uint maxlength __attribute__((unused))) -> int

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
inline int untr_decode_audio4(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt, uint maxlength) {
	int consumed = avcodec_decode_audio4(avctx, frame, got_frame, pkt);

	// ffmpeg 3.4+ uses internal buffer which needs to be updated.
	// this is slow because of the internal memory allocation.
	// ff34+ decodes till exhaustion, which in turn spams repetitive warnings/errors
	if (is_new_ffmpeg_api && consumed < 0) {
		if (!g_dont_omit && !g_muted) {
			logg(I, "Muted ffmpeg to reduce redundant warnings/errors. Use '-do' to see them.\n");
			mute();  // don't spam libav warnings/errors
		}
		avcodec_flush_buffers(avctx);
		pkt->size = maxlength;
		consumed = avcodec_decode_audio4(avctx, frame, got_frame, pkt);
		if (consumed < 0) {
			avcodec_flush_buffers(avctx);
		}
	}
	return consumed;
}

inline int untr_decode_video2(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt) {
	// ffmpeg3.4+ expects every video packet to contain exactly 1 frame..
	// thus there is no way to detect video frame bounderies using ffmpeg3.4+..
	// https://github.com/FFmpeg/FFmpeg/blob/7fc329e2dd6226dfecaa4a1d7adf353bf2773726/libavcodec/avcodec.h#L4793
	// https://github.com/FFmpeg/FFmpeg/commit/061a0c14bb5767bca72e3a7227ca400de439ba09
	return avcodec_decode_video2(avctx, frame, got_frame, pkt);
}
#pragma GCC diagnostic pop

map<string, int(*) (Codec*, const uchar*, uint maxlength)> dispatch_get_size {
	GET_SZ_FN("mp4a") {
		start_m4pa:
		maxlength = min(g_max_buf_sz_needed, maxlength);

		static AVPacket* packet = av_packet_alloc();
//		packet->size = g_max_partsize;
		static AVFrame* frame = av_frame_alloc();

		packet->data = const_cast<uchar*>(start);
		if (!is_new_ffmpeg_api) packet->size = maxlength;
		int got_frame = 0;

		int consumed = untr_decode_audio4(self->av_codec_context_, frame, &got_frame, packet, maxlength);
//		int consumed = avcodec_decode_audio4(context_, frame_, &got_frame, packet);

		self->audio_duration_ = frame->nb_samples;
		logg(V, "nb_samples: ", self->audio_duration_, '\n');

		self->was_bad_ = (!got_frame || self->av_codec_params_->channels != frame->channels);
		if (self->was_bad_) {
			logg(V, "got_frame: ", got_frame, '\n');
			logg(V, "channels: ", self->av_codec_params_->channels, ", ", frame->channels, '\n');

			if (is_new_ffmpeg_api && to_uint(packet->size) != maxlength) {
				logg(V, "avcodec_flush_buffers, then retry..\n");
				avcodec_flush_buffers(self->av_codec_context_);
				packet->size = maxlength;
				goto start_m4pa;
			}
		}

		// simulate state for new API
		if (is_new_ffmpeg_api && consumed >= 0) {
			packet->size -= consumed;
			if (packet->size <= 0) packet->size = maxlength;
		}

		return consumed;
	}},

    {"avc1", getSizeAvc1},
    {"hvc1", getSizeHvc1},
//    GET_SZ_FN("avc1") {
		//        AVFrame *frame = av_frame_alloc();
		//        if(!frame)
		//            throw "Could not create AVFrame";
		//        AVPacket avp;
		//        av_init_packet(&avp);

		//        int got_frame;
		//        avp.data=(uchar *)(start);
		//        avp.size = maxlength;

		/*
		int consumed = avcodec_decode_video2(context, frame, &got_frame, &avp);
		cout << "Consumed: " << consumed << endl;
		av_freep(&frame);
		cout << "Consumed: " << consumed << endl;

		return consumed;
		*/
		//first 4 bytes are the length, then the nal starts.
		//ref_idc !=0 per unit_type = 5
		//ref_idc == 0 per unit_type = 6, 9, 10, 11, 12
//	}},

	GET_SZ_FN("samr") { //lenght is multiple of 32, we split packets.
		return 32;
	}},
	GET_SZ_FN("apcn") {
		return swap32(*(int *)start);
	}},
	GET_SZ_FN("tmcd") {  // GoPro timecode, always 4 bytes, only pkt-idx 4 (?)
//		tmcd_seen_ = true;
		return 4;
	}},
	GET_SZ_FN("fdsc") {  // GoPro recovery
		// TODO: How is this track used for recovery?

		static int fdsc_idx_ = -1;
		fdsc_idx_++;
		if (fdsc_idx_ == 0) {
			for(auto pos=start+4; maxlength; pos+=4, maxlength-=4) {
				if (string((char*)pos, 2) == "GP") return pos-start;
			}
		}
		else if (fdsc_idx_ == 1) return g_mp4->getTrack("fdsc").getOrigSize(1);  // probably 152 ?
		return 16;
	}},
	GET_SZ_FN("gpmd") {  // GoPro meta data, see 'gopro/gpmf-parser'
		int s2 = swap32(((int *)start)[1]);
		int num = (s2 & 0x0000ffff);
		return num + 8;
	}},
	GET_SZ_FN("mp4v") {
		static AVPacket* packet = av_packet_alloc();
		static AVFrame* frame = av_frame_alloc();

		packet->data = const_cast<uchar*>(start);
		maxlength = min(g_max_buf_sz_needed, maxlength);
		packet->size = maxlength;
		int got_frame = 0;

		int consumed = untr_decode_video2(self->av_codec_context_, frame, &got_frame, packet);

		self->was_keyframe_ = frame->key_frame;
		self->was_bad_ = !got_frame;

		return consumed;
	}},
	GET_SZ_FN("mebx") {
		return swap32(*(int *)start);
	}},
	GET_SZ_FN("icod") {
		return swap32(*(int *)(start+2));
	}},
	GET_SZ_FN("ap4x") {
		return swap32(*(int *)start);
	}},
	GET_SZ_FN("camm") {
		// https://developers.google.com/streetview/publish/camm-spec
		// https://github.com/ponchio/untrunc/blob/2f4de8aa/codec_camm.cpp#L12
		int lengths[] = { 12, 8, 12, 12, 12, 24, 14*4, 12 };
		int type = start[2];
		return lengths[type] + 4;
	}},
	GET_SZ_FN("jpeg") {
		int n = 0, last_load = 0;
		auto p = start;
		while (1) {
			if (to_uint(n) - last_load > g_max_buf_sz_needed / 2) {
				p = self->loadAfter(n);
				last_load = n;
			}

			uchar t = p[1];  // https://www.disktuna.com/list-of-jpeg-markers/
			if (p[0] == 0xff && !(t <= 0x01) && !(0xd0 <= t && t <= 0xd8)) {
				if (t == 0xd9) return n + 2;
				int len = swap16(*((int16_t*)(p+2)));
				n += len - 2;
				p += len - 2;
			}
			else {
				n++;
				p++;
			}
		}
	}},

	/* if codec is not found in map,
	 * untrunc will try to generate common features (chunk_size, sample_size, patterns) per track.
	 * this is why these are commented out
	 *
	GET_SZ_FN("twos") { //lenght is multiple of 32, we split packets.
		return 4;
	}},
	GET_SZ_FN("in24") {
		return -1;
	}},
	GET_SZ_FN("lpcm") {
		// Use hard-coded values for now....
		const int num_samples      = 4096; // Empirical
		const int num_channels     =    2; // Stereo
		const int bytes_per_sample =    2; // 16-bit
		return num_samples * num_channels * bytes_per_sample;
	}},
	*/
};

int Codec::getSize(const uchar* start, uint maxlength, off_t offset) {
	cur_off_ = offset;
	return get_size_fn_ ? get_size_fn_(this, start, maxlength) : -1;
}

const uchar* Codec::loadAfter(off_t length) {
	return g_mp4->loadFragment(cur_off_ + length, false);
}
