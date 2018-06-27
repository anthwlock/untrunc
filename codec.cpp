#include "codec.h"

#include <iostream>
#include <vector>
#include <cassert>

extern "C" {
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "common.h"
#include "atom.h"
#include "nal.h"
#include "sps-info.h"
#include "nal-slice.h"
#include "avc-config.h"

using namespace std;

Codec::Codec(AVCodecContext* c) : avc_config_(NULL) {
	context_ = c;

	//    codec_.parse(trak, offsets, mdat);
	codec_ = avcodec_find_decoder(context_->codec_id);
	//if audio use next?

	if (!codec_)
		throw string("No codec found!");
	if (avcodec_open2(context_, codec_, NULL)<0)
		throw string("Could not open codec: ?"); //+ context_->codec_name;

}

void Codec::parse(Atom *trak, vector<int> &offsets, Atom *mdat) {
	Atom *stsd = trak->atomByName("stsd");
	int entries = stsd->readInt(4);
	if(entries != 1)
		throw string("Multiplexed stream! Not supported");

	char codec[5];
	stsd->readChar(codec, 12, 4);
	name_ = codec;
//	cout << "codec: " <<  codec << '\n';

	if (codec == string("avc1")) {
		avc_config_ = new AvcConfig(*stsd);
		if (!avc_config_->is_ok)
			logg(W, "avcC was not decoded correctly\n");
		else
			logg(I, "avcC got decoded\n");
	}

	//this was a stupid attempt at trying to detect packet type based on bitmasks
	mask1_ = 0xffffffff;
	mask0_ = 0xffffffff;
	//build the mask:
	for(int i = 0; i < offsets.size(); i++) {
		int offset = offsets[i];
		if(offset < mdat->start_ || offset - mdat->start_ > mdat->length_) {
			cout << "i = " << i;
			cout << "\noffset = " << offset
				 << "\nmdat->start = " << mdat->start_
				 << "\nmdat->length = " << mdat->length_
				 << "\noffset - mdat->start = " << offset - mdat->start_;
			cout << "\nInvalid offset in track!\n";
			exit(0);
		}

		int s = mdat->readInt(offset - mdat->start_ - 8);
		mask1_ &= s;
		mask0_ &= ~s;

		assert((s & mask1_) == mask1_);
		assert((~s & mask0_) == mask0_);
	}
}

bool Codec::matchSample(const uchar *start) {
	int s = swap32(*(int *)start);
	if(name_ == "avc1") {

		//this works only for a very specific kind of video
		//#define SPECIAL_VIDEO
#ifdef SPECIAL_VIDEO
		int s2 = swap32(((int *)start)[1]);
		if(s != 0x00000002 || (s2 != 0x09300000 && s2 != 0x09100000)) return false;
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
		logg(V, "avc1: failed for not particular reason\n");
		return false;

	} else if(name_ == "mp4a") {
		if(s > 1000000) {
			logg(V, "mp4a: Success because of large s value\n");
			return true;
		}
		//horrible hack... these values might need to be changed depending on the file
		if((start[4] == 0xee && start[5] == 0x1b) ||
		   (start[4] == 0x3e && start[5] == 0x64)) {
			cout << "mp4a: Success because of horrible hack.\n";
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

	} else if(name_ == "mp4v") { //as far as I know keyframes are 1b3, frames are 1b6 (ISO/IEC 14496-2, 6.3.4 6.3.5)
		if(s == 0x1b3 || s == 0x1b6)
			return true;
		return false;

	} else if(name_ == "alac") {
		int t = swap32(*(int *)(start + 4));
		t &= 0xffff0000;

		if(s == 0 && t == 0x00130000) return true;
		if(s == 0x1000 && t == 0x001a0000) return true;
		return false;

	} else if(name_ == "samr") {
		return start[0] == 0x3c;
	} else if(name_ == "twos") {
		//weird audio codec: each packet is 2 signed 16b integers.
		cerr << "This audio codec is EVIL, there is no hope to guess it.\n";
		exit(0);
		return true;
	} else if(name_ == "apcn") {
		return memcmp(start, "icpf", 4) == 0;
	} else if(name_ == "lpcm") {
		// This is not trivial to detect because it is just
		// the audio waveform encoded as signed 16-bit integers.
		// For now, just test that it is not "apcn" video:
		return memcmp(start, "icpf", 4) != 0;
	} else if(name_ == "in24") { //it's a codec id, in a case I found a pcm_s24le (little endian 24 bit) No way to know it's length.
		return true;
	} else if(name_ == "sowt") {
		cerr << "Sowt is just  raw data, no way to guess length (unless reliably detecting the other codec start)\n";
		return false;
	}

	return false;
}

int Codec::getLength(const uchar *start, int maxlength, int &duration) {
	if(name_ == "mp4a") {
		AVFrame *frame = av_frame_alloc();
		if(!frame)
			throw string("Could not create AVFrame");
		AVPacket avp;
		av_init_packet(&avp);

		int got_frame;
		avp.data = const_cast<uchar*>(start);
		avp.size = maxlength;

//		cout << "buffer:\n";
//		printBuffer(start, 30);

		int consumed = avcodec_decode_audio4(context_, frame, &got_frame, &avp);
		duration = frame->nb_samples;
		logg(V, "N(Samples): ", frame->nb_samples, '\n');
		av_freep(&frame);
		return consumed;

	} else if(name_ == "mp4v") {

		/*     THIS DOES NOT SEEM TO WORK FOR SOME UNKNOWN REASON. IT JUST CONSUMES ALL BYTES.
		*     AVFrame *frame = avcodec_alloc_frame();
		if(!frame)
			throw string("Could not create AVFrame");
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

	} else if(name_ == "avc1") {
		//        AVFrame *frame = av_frame_alloc();
		//        if(!frame)
		//            throw string("Could not create AVFrame");
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
			if (avc_config_->sps_info_->is_ok){
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
		last_frame_was_idr_ = false;

		while(1) {
			logg(V, "---\n");
			NalInfo nal_info(pos, maxlength);
			if(!nal_info.is_ok){
				logg(V, "failed parsing nal-header\n");
				return length;
			}

			switch(nal_info.nal_type) {
			case NAL_SPS:
				if(previous_slice.is_ok){
					logg(E, "searching end, found new 'SPS'\n");
					return length;
				}
				if (!sps_info.is_ok)
					sps_info.decode(nal_info.payload_);
				break;
			case NAL_AUD: // Access unit delimiter
				if (!previous_slice.is_ok)
					break;
				return length;
			case NAL_IDR_SLICE:
				last_frame_was_idr_ = true; // keyframe
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
					if(previous_nal.ref_idc != nal_info.ref_idc &&
					   (previous_nal.ref_idc == 0 || nal_info.ref_idc == 0)) {
						logg(W, "Different ref idc\n");
						return length;
					}
				}
				break;
			}
			default:
				if(previous_slice.is_ok) {
					if(!nal_info.is_forbidden_set_) {
						logg(E, "New access unit since seen picture (type: ", nal_info.nal_type, ")\n");
						return length;
					} // else is malformed, no stand-alone frame
				}
				break;
			}
			pos += nal_info.length;
			length += nal_info.length;
			maxlength -= nal_info.length;
			if (maxlength == 0) // we made it
				return length;
			logg(V, "Partial avc1-length: ", length, "\n");
		}
		return length;

	} else if(name_ == "samr") { //lenght is multiple of 32, we split packets.
		return 32;
	} else if(name_ == "twos") { //lenght is multiple of 32, we split packets.
		return 4;
	} else if(name_ == "apcn") {
		return swap32(*(int *)start);
	} else if(name_ == "lpcm") {
		// Use hard-coded values for now....
		const int num_samples      = 4096; // Empirical
		const int num_channels     =    2; // Stereo
		const int bytes_per_sample =    2; // 16-bit
		return num_samples * num_channels * bytes_per_sample;
	} else if(name_ == "in24") {
		return -1;
	} else
		return -1;
}
