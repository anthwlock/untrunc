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
#include "nal.h"
#include "sps-info.h"
#include "nal-slice.h"
#include "avc-config.h"
#include "mp4.h"

using namespace std;

Codec::Codec(AVCodecParameters* c) : av_codec_params_(c) {
	av_codec_ = avcodec_find_decoder(c->codec_id);

	if (!av_codec_) {
		auto codec_type = av_get_media_type_string(c->codec_type);
		auto codec_name = avcodec_get_name(c->codec_id);
		logg(V, "FFmpeg does not support codec: <", codec_type, ", ", codec_name, ">\n");
		return;
	}

	av_codec_context_ = avcodec_alloc_context3(av_codec_);
	avcodec_parameters_to_context(av_codec_context_, c);

	if (avcodec_open2(av_codec_context_, av_codec_, NULL) < 0)
		throw "Could not open codec: ?";

}

void Codec::parse(Atom *trak, const vector<off_t>& offsets, Atom *mdat) {
	Atom *stsd = trak->atomByName("stsd");
	int entries = stsd->readInt(4);
	if(entries != 1)
		throw "Multiplexed stream! Not supported";

	name_ = stsd->getString(12, 4);

	if (name_ == "avc1") {
		avc_config_ = new AvcConfig(*stsd);
		if (!avc_config_->is_ok)
			logg(W, "avcC was not decoded correctly\n");
		else
			logg(V, "avcC got decoded\n");
	}

	else if (name_ == "mp4a") {
		frame_ = av_frame_alloc();
		packet_ = av_packet_alloc();
		packet_->size = g_max_partsize;
	}

	for (uint i = 0; i < offsets.size(); i++) {
		int64_t offset = offsets[i];
		if (offset < mdat->start_ || offset - mdat->start_ > mdat->length_) {
			cout << "i = " << i;
			cout << "\noffset = " << offset
			     << "\nmdat->start = " << mdat->start_
			     << "\nmdat->length = " << mdat->length_
			     << "\noffset - mdat->start = " << offset - mdat->start_;
			cout << "\nInvalid offset in track!\n";
			exit(0);
		}
	}
}

// only match if we are certain -> less false positives
// you might want to tweak these values..
bool Codec::matchSampleStrict(const uchar *start) const {
	int s = swap32(*(int *)start);  // big endian

	if (name_ == "avc1") {
		if (strictness_level_ > 0) {
			int s2 = swap32(((int *)start)[1]);
			return s == 0x00000002 && (s2 == 0x09300000 || s2 == 0x09100000);
		}
		int s1 = s;
		return s1 == 0x01 || s1 == 0x02 || s1 == 0x03;
	}
	else if (name_ == "mp4a") {
		return false;
//		return (s>>16) == 0x210A;  // this needs to be improved
	}
	else if (name_ == "tmcd") {
		return false;
	}
	else {
		return matchSample(start);
	}

}

bool Codec::matchSample(const uchar *start) const{
	int s = swap32(*(int *)start);  // big endian
	if (name_ == "avc1") {

		//this works only for a very specific kind of video
		//#define SPECIAL_VIDEO
#ifdef SPECIAL_VIDEO
		int s2 = swap32(((int *)start)[1]);
		return (s != 0x00000002 || (s2 != 0x09300000 && s2 != 0x09100000)) return false;
		return true;
#endif


		//TODO use the first byte of the nal: forbidden bit and type!
		int nal_type = (start[4] & 0x1f);
		//the other values are really uncommon on cameras...
		if(nal_type > 21) {
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

	} else if (name_ == "mp4a") {
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

	} else if (name_ == "mp4v") { //as far as I know keyframes are 1b3, frames are 1b6 (ISO/IEC 14496-2, 6.3.4 6.3.5)
		if(s == 0x1b3 || s == 0x1b6)
			return true;
		return false;

	} else if (name_ == "alac") {
		int t = swap32(*(int *)(start + 4));
		t &= 0xffff0000;

		if(s == 0 && t == 0x00130000) return true;
		if(s == 0x1000 && t == 0x001a0000) return true;
		return false;

	} else if (name_ == "samr") {
		return start[0] == 0x3c;
	} else if (name_ == "twos") {
		//weird audio codec: each packet is 2 signed 16b integers.
		cerr << "This audio codec is EVIL, there is no hope to guess it.\n";
		exit(0);
		return true;
	} else if (name_ == "apcn") {
		return memcmp(start, "icpf", 4) == 0;
	} else if (name_ == "lpcm") {
		// This is not trivial to detect because it is just
		// the audio waveform encoded as signed 16-bit integers.
		// For now, just test that it is not "apcn" video:
		return memcmp(start, "icpf", 4) != 0;
	} else if (name_ == "in24") { //it's a codec id, in a case I found a pcm_s24le (little endian 24 bit) No way to know it's length.
		return true;
	} else if (name_ == "sowt") {
		cerr << "Sowt is just  raw data, no way to guess length (unless reliably detecting the other codec start)\n";
		return false;
	} else if (name_ == "sawb") {
		return start[0] == 0x44;
	}
	if (name_ == "tmcd") {  // GoPro timecode .. hardcoded in Mp4::GetMatches
		return false;
//		return !tmcd_seen_ && start[0] == 0 && g_mp4->wouldMatch(start+4, "tmcd");
	}
	if (name_ == "gpmd") {  // GoPro timecode, 4 bytes (?)
		vector<string> fourcc = {"DEVC", "DVID", "DVNM", "STRM", "STNM", "RMRK",  "SCAL",
		                         "SIUN", "UNIT", "TYPE", "TSMP", "TIMO", "EMPT"};
		return contains(fourcc, string((char*)start, 4));
	}
	if (name_ == "fdsc") {  // GoPro recovery, anyone knows more?
		return string((char*)start, 2) == "GP";
	}

	return false;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
inline int untr_decode_audio4(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt, uint maxlength)
{
	int consumed = avcodec_decode_audio4(avctx, frame, got_frame, pkt);

	// ffmpeg 3.4+ uses internal buffer which needs to be updated.
	// this is slow because of the internal memory allocation.
	// ff34+ decodes till exhaustion, which in turn spams repetitive warnings/errors
	if (is_new_ffmpeg_api && consumed < 0) {
		if (g_log_mode < LogMode::V && !g_muted) {
			logg(I, "Muted ffmpeg to reduce redundant warnings/errors. Use '-v' to see them.\n");
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
#pragma GCC diagnostic pop


int Codec::getSize(const uchar *start, uint maxlength) {
	if (name_ == "mp4a" || name_ == "sawb") {
		packet_->data = const_cast<uchar*>(start);
		if(!is_new_ffmpeg_api) packet_->size = maxlength;
		int got_frame = 0;

		int consumed = untr_decode_audio4(av_codec_context_, frame_, &got_frame, packet_, maxlength);
//		int consumed = avcodec_decode_audio4(context_, frame_, &got_frame, packet_);

		// simulate state for new API
		if (is_new_ffmpeg_api && consumed >= 0) {
			packet_->size -= consumed;
			if (packet_->size <= 0) packet_->size = maxlength;
		}

		audio_duration_ = frame_->nb_samples;
		logg(V, "nb_samples: ", audio_duration_, '\n');

		if (!n_channels_) n_channels_ = frame_->channels;
		was_bad_ = (!got_frame || frame_->channels != n_channels_);

		return consumed;

	} else if (name_ == "mp4v") {

		/*     THIS DOES NOT SEEM TO WORK FOR SOME UNKNOWN REASON. IT JUST CONSUMES ALL BYTES.
		*     AVFrame *frame = avcodec_alloc_frame();
		if(!frame)
			throw "Could not create AVFrame";
		AVPacket avp;
		av_init_packet(&avp);

		int got_frame;
		avp.data=(uchar *)(start);
		avp.size = maxlength;
		int consumed = avcodec_decode_video2(context, frame, &got_frame, &avp);
		av_freep(&frame);
		return consumed; */

		//found no way to guess size, probably the only way is to use some functions in ffmpeg
		//to decode the stream
		cout << "Unfortunately I found no way to guess size of mp4v packets. Sorry\n";
		return -1;

	} else if (name_ == "avc1") {
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

		uint32_t length = 0;
		const uchar *pos = start;

		static bool sps_info_initialized = false;
		static SpsInfo sps_info;
		if (!sps_info_initialized){
			logg(V, "sps_info (before): ",
			     sps_info.frame_mbs_only_flag,
			     ' ', sps_info.log2_max_frame_num,
			     ' ', sps_info.log2_max_poc_lsb,
			     ' ', sps_info.poc_type, '\n');
			if (avc_config_->is_ok){
				sps_info = *avc_config_->sps_info_;
			}
			sps_info_initialized = true;
			logg(V, "sps_info (after):  ",
			     sps_info.frame_mbs_only_flag,
			     ' ', sps_info.log2_max_frame_num,
			     ' ', sps_info.log2_max_poc_lsb,
			     ' ', sps_info.poc_type, '\n');
		}

		SliceInfo previous_slice;
		NalInfo previous_nal;
		was_keyframe_ = false;

		while(1) {
			logg(V, "---\n");
			NalInfo nal_info(pos, maxlength);
			if(!nal_info.is_ok){
				logg(V, "failed parsing nal-header\n");
				return length;
			}

			switch(nal_info.nal_type_) {
			case NAL_SPS:
				if(previous_slice.is_ok){
					logg(W2, "searching end, found new 'SPS'\n");
					return length;
				}
				if (!sps_info.is_ok)
					sps_info.decode(nal_info.data_);
				break;
			case NAL_AUD: // Access unit delimiter
				if (!previous_slice.is_ok)
					break;
				return length;
			case NAL_IDR_SLICE:
				was_keyframe_ = true; // keyframe
				[[fallthrough]];
			case NAL_SLICE:
			{
				SliceInfo slice_info(nal_info, sps_info);
				if(!previous_slice.is_ok){
					previous_slice = slice_info;
					previous_nal = move(nal_info);
				}
				else {
					if (slice_info.isInNewFrame(previous_slice))
						return length;
					if(previous_nal.ref_idc_ != nal_info.ref_idc_ &&
					   (previous_nal.ref_idc_ == 0 || nal_info.ref_idc_ == 0)) {
						logg(W, "Different ref idc\n");
						return length;
					}
				}
				break;
			}
			case NAL_FILLER_DATA:
				if (g_log_mode >= V) {
					logg(V, "found filler data: ");
					printBuffer(pos, 30);
				}
				break;
			default:
				if(previous_slice.is_ok) {
					if(!nal_info.is_forbidden_set_) {
						logg(W2, "New access unit since seen picture (type: ", nal_info.nal_type_, ")\n");
						return length;
					} // otherwise it's malformed, don't produce an isolated malformed unit
				}
				break;
			}
			pos += nal_info.length_;
			length += nal_info.length_;
			maxlength -= nal_info.length_;
			if (maxlength == 0) // we made it
				return length;
			logg(V, "Partial avc1-length: ", length, "\n");
		}
		return length;

	} else if (name_ == "samr") { //lenght is multiple of 32, we split packets.
		return 32;
	} else if (name_ == "twos") { //lenght is multiple of 32, we split packets.
		return 4;
	} else if (name_ == "apcn") {
		return swap32(*(int *)start);
	} else if (name_ == "lpcm") {
		// Use hard-coded values for now....
		const int num_samples      = 4096; // Empirical
		const int num_channels     =    2; // Stereo
		const int bytes_per_sample =    2; // 16-bit
		return num_samples * num_channels * bytes_per_sample;
	} else if (name_ == "in24") {
		return -1;
	} else if (name_ == "tmcd") {  // GoPro timecode, always 4 bytes, only pkt-idx 4 (?)
//		tmcd_seen_ = true;
		return 4;
	} else if (name_ == "gpmd") {  // GoPro meta data, see 'gopro/gpmf-parser'
		int s2 = swap32(((int *)start)[1]);
		int num = (s2 & 0x0000ffff);
		return num + 8;
	} else if (name_ == "fdsc") {  // GoPro recovery
		// TODO: How is this track used for recovery?

		fdsc_idx_++;
		if (fdsc_idx_ == 0) {
			for(auto pos=start+4; maxlength; pos+=4, maxlength-=4) {
				if (string((char*)pos, 2) == "GP") return pos-start;
			}
		}
		else if (fdsc_idx_ == 1) return g_mp4->getTrack("fdsc").getOrigSize(1);  // probably 152 ?
		return 16;

	}
	else return -1;
}
