/*
	Untrunc - track.cpp

	Untrunc is GPL software; you can freely distribute,
	redistribute, modify & use under the terms of the GNU General
	Public License; either version 2 or its successor.

	Untrunc is distributed under the GPL "AS IS", without
	any warranty; without the implied warranty of merchantability
	or fitness for either an expressed or implied particular purpose.

	Please see the included GNU General Public License (GPL) for
	your rights and further details; see the file COPYING. If you
	cannot, write to the Free Software Foundation, 59 Temple Place
	Suite 330, Boston, MA 02111-1307, USA.  Or www.fsf.org

	Copyright 2010 Federico Ponchio

														*/

#include "track.h"
#include "atom.h"

#include <iostream>
#include <vector>
#include <string.h>
#include <assert.h>
#include <endian.h>

#define __STDC_LIMIT_MACROS 1
#define __STDC_CONSTANT_MACROS 1

extern "C" {
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

//Horrible hack: there is a variabled named 'new' and 'class' inside!
#if LIBAVCODEC_VERSION_MAJOR != 56 //ubuntu 16.04 version
#include <config.h>
#undef restrict
//#define restrict __restrict__
#define restrict
#define new extern_new
#define class extern_class
#include <libavcodec/h264dec.h>
#undef new
#undef class
#undef restrict

#else
define new extern_new
#define class extern_class
#include <libavcodec/h264dec.h>
#undef new
#undef class
#endif

}

using namespace std;


void Codec::parse(Atom *trak, vector<int> &offsets, Atom *mdat) {
	Atom *stsd = trak->atomByName("stsd");
	int entries = stsd->readInt(4);
	if(entries != 1)
		throw string("Multiplexed stream! Not supported");

	char _codec[5];
	stsd->readChar(_codec, 12, 4);
	name = _codec;


	//this was a stupid attempt at trying to detect packet type based on bitmasks
	mask1 = 0xffffffff;
	mask0 = 0xffffffff;
	//build the mask:
	for(int i = 0; i < offsets.size(); i++) {
		int offset = offsets[i];
		if(offset < mdat->start || offset - mdat->start > mdat->length) {
			cout << "Invalid offset in track!\n";
			exit(0);
		}

		int s = mdat->readInt(offset - mdat->start - 8);
		mask1 &= s;
		mask0 &= ~s;

		assert((s & mask1) == mask1);
		assert((~s & mask0) == mask0);
	}
}

#define VERBOSE 1

bool Codec::matchSample(unsigned char *start, int maxlength) {
	int s = swap32(*(int *)start);

	if(name == "avc1") {

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
#ifdef VERBOSE
			cout << "avc1: no match because of nal type: " << nal_type << endl;
#endif
			return false;
		}
		//if nal is equal 7, the other fragments (starting with nal type 7)
		//should be part of the same packet
		//(we cannot recover time information, remember)
		if(start[0] == 0) {
#ifdef VERBOSE
			cout << "avc1: Match with 0 header\n";
#endif
			return true;
		}
#ifdef VERBOSE
		cout << "avc1: failed for not particular reason\n";
#endif
		return false;

	} else if(name == "mp4a") {
		if(s > 1000000) {
#ifdef VERBOSE
			cout << "mp4a: Success because of large s value\n";
#endif
			return true;
		}
		//horrible hack... these values might need to be changed depending on the file
		if((start[4] == 0xee && start[5] == 0x1b) ||
				(start[4] == 0x3e && start[5] == 0x64)) {
			cout << "mp4a: Success because of horrible hack.\n";
			return true;
		}

		if(start[0] == 0) {
#ifdef VERBOSE
			cout << "Failure because of NULL header\n";
			return false;
#endif
		}
#ifdef VERBOSE
		cout << "Success for no particular reason....\n";
#endif
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

	} else if(name == "mp4v") { //as far as I know keyframes are 1b3, frames are 1b6 (ISO/IEC 14496-2, 6.3.4 6.3.5)
		if(s == 0x1b3 || s == 0x1b6)
			return true;
		return false;

	} else if(name == "alac") {
		int t = swap32(*(int *)(start + 4));
		t &= 0xffff0000;

		if(s == 0 && t == 0x00130000) return true;
		if(s == 0x1000 && t == 0x001a0000) return true;
		return false;

	} else if(name == "samr") {
		return start[0] == 0x3c;
	} else if(name == "twos") {
		//weird audio codec: each packet is 2 signed 16b integers.
		cerr << "This audio codec is EVIL, there is no hope to guess it.\n";
		exit(0);
		return true;
	} else if(name == "apcn") {
		return memcmp(start, "icpf", 4) == 0;
	} else if(name == "lpcm") {
		// This is not trivial to detect because it is just
		// the audio waveform encoded as signed 16-bit integers.
		// For now, just test that it is not "apcn" video:
		return memcmp(start, "icpf", 4) != 0;
	} else if(name == "in24") { //it's a codec id, in a case I found a pcm_s24le (little endian 24 bit) No way to know it's length.
		return true;
	} else if(name == "sowt") {
		cerr << "Sowt is just  raw data, no way to guess length (unless reliably detecting the other codec start)\n";
		return false;
	}

	return false;
}

//AVC1

int golomb(uint8_t *&buffer, int &offset) {
	//count the zeroes;
	int count = 0;
	//count the leading zeroes
	while((*buffer & (0x1<<(7 - offset))) == 0) {
		count++;
		offset++;
		if(offset == 8) {
			buffer++;
			offset = 0;
		}
		if(count > 20) {
			cout << "Failed reading golomb: too large!\n";
			return -1;
		}
	}
	//skip the single 1 delimiter
	offset++;
	if(offset == 8) {
		buffer++;
		offset = 0;
	}
	uint32_t res = 1;
	//read count bits
	while(count-- > 0) {
		res <<= 1;
		res |= (*buffer  & (0x1<<(7 - offset))) >> (7 - offset);
		offset++;
		if(offset == 8) {
			buffer++;
			offset = 0;
		}
	}
	return res-1;
}


int readBits(int n, uint8_t *&buffer, int &offset) {
	int res = 0;
	while(n + offset > 8) { //can't read in a single reading
		int d = 8 - offset;
		res <<= d;
		res |= *buffer & ((1<<d) - 1);
		offset = 0;
		buffer++;
		n -= d;
	}
	//read the remaining bits
	int d = (8 - offset - n);
	res <<= n;
	res |= (*buffer >> d) & ((1 << n) - 1);
	return res;
}

class NalInfo {
public:
	int length;

	int ref_idc;
	int nal_type;
	int first_mb; //unused
	int slice_type;   //should match the nal type (1, 5)
	int pps_id;       //which parameter set to use
	int frame_num;
	int field_pic_flag;
	int bottom_pic_flag;
	int idr_pic_flag; //actually 1 for nal_type 5, 0 for nal_type 0
	int idr_pic_id;   //read only for nal_type 5
	int poc_type; //if zero check the lsb
	int poc_lsb;
	NalInfo(): length(0), ref_idc(0), nal_type(0), first_mb(0), slice_type(0), pps_id(0),
		frame_num(0), field_pic_flag(0), bottom_pic_flag(0), idr_pic_flag(0), idr_pic_id(0),
		poc_type(0), poc_lsb(0) {}
};

class H264sps {
public:
	int log2_max_frame_num;
	bool frame_mbs_only_flag;
	int poc_type;
	int log2_max_poc_lsb;
};

H264sps parseSPS(uint8_t *data, int size) {
	if (data[0] != 1)  {
		cerr << "Uncharted territory..." << endl;
	}
	H264sps sps;
	int i, cnt, nalsize;
	const uint8_t *p = data;

	if (size < 7) {
		cerr << "Could not parse SPS!" << endl;
		exit(0);
	}

	// Decode sps from avcC
	cnt = *(p + 5) & 0x1f; // Number of sps
	p  += 6;
	if(cnt != 1) {
		cerr << "Not supporting more than 1 SPS unit for the moment, might fail horribly." << endl;
	}
	for (i = 0; i < cnt; i++) {
		//nalsize = AV_RB16(p) + 2;
		uint16_t n = *(uint16_t *)p;
		nalsize = (n>>8) | (n<<8);
		if (p - data + nalsize > size) {
			cerr << "Could not parse SPS!" << endl;
			exit(0);
		}
/*		ret = decode_extradata_ps_mp4(p, nalsize, ps, err_recognition, logctx);
		if (ret < 0) {
			av_log(logctx, AV_LOG_ERROR,
				   "Decoding sps %d from avcC failed\n", i);
			return ret;
		}
		p += nalsize; */
		break;
	}
	//Skip pps

	return sps;
}


//return false means this probably is not a nal.
bool getNalInfo(H264sps &sps, uint32_t maxlength, uint8_t *buffer, NalInfo &info) {

	if(buffer[0] != 0) {
		cout << "First byte expected 0\n";
		return false;
	}
	//this is supposed to be the length of the NAL unit.
	uint32_t len = swap32(*(uint32_t *)buffer);

	int MAX_AVC1_LENGTH = 8*(1<<20);
	if(len > MAX_AVC1_LENGTH) {
		cout << "Max length exceeded\n";
		return false;
	}

	if(len + 4 > maxlength) {
		cout << "Buffer size exceeded\n";
		return false;
	}
	info.length = len + 4;
	cout << "Length: " << info.length << "\n";

	buffer += 4;
	if(*buffer & (1 << 7)) {
		cout << "Forbidden first bit 1\n";
		return false; //forbidden first bit;
	}
	info.ref_idc = *buffer >> 5;
	cout << "Ref idc: " << info.ref_idc << "\n";

	info.nal_type = *buffer & 0x1f;
	cout << "Nal type: " << info.nal_type << "\n";
	if(info.nal_type != 1 && info.nal_type != 5)
		return true;

	//check size is reasonable:
	if(len < 8) {
		cout << "Too short!\n";
		return false;
	}

	buffer++; //skip nal header

	//remove the emulation prevention 3 byte.
	//could be done in place to speed up things.
	vector<uint8_t> data;
	data.reserve(len);
	for(int i =0; i < len; i++) {
		if(i+2 < len && buffer[i] == 0 && buffer[i+1] == 0 && buffer[i+2] == 3) {
			data.push_back(buffer[i]);
			data.push_back(buffer[i+1]);
			assert(buffer[i+2] == 0x3);
			i += 2; //skipping 0x3 byte!
		} else
			data.push_back(buffer[i]);
	}

	uint8_t *start = data.data();
	int offset = 0;
	info.first_mb = golomb(start, offset);
	//TODO is there a max number (so we could validate?)
	cout << "First mb: " << info.first_mb << endl; "\n";

	info.slice_type = golomb(start, offset);
	if(info.slice_type > 9) {
		cout << "Invalid slice type, probably this is not an avc1 sample\n";
		return false;
	}
	info.pps_id = golomb(start, offset);
	cout << "pic paramter set id: " << info.pps_id << "\n";
	//pps id: should be taked from master context (h264_slice.c:1257

	//assume separate coloud plane flag is 0
	//otherwise we would have to read colour_plane_id which is 2 bits

	//assuming same sps for all frames:
	//SPS *sps = (SPS *)(h->ps.sps_list[0]->data);
	info.frame_num = readBits(sps.log2_max_frame_num, start, offset);
	cout << "Frame num: " << info.frame_num << "\n";

	//read 2 flags
	info.field_pic_flag = 0;
	info.bottom_pic_flag = 0;
	if(sps.frame_mbs_only_flag) {
		info.field_pic_flag = readBits(1, start, offset);
		cout << "field: " << info.field_pic_flag << "\n";
		if(info.field_pic_flag) {
			info.bottom_pic_flag = readBits(1, start, offset);
			cout << "bottom: " << info.bottom_pic_flag << "\n";
		}
	}
	info.idr_pic_flag = (info.nal_type == 5)? 1 : 0;
	if (info.nal_type == 5 ) {
		info.idr_pic_id = golomb(start, offset);
		cout << "Idr pic: " << info.idr_pic_id << "\n";
	}

	//if pic order cnt type == 0
	if(sps.poc_type == 0) {
		info.poc_lsb = readBits(sps.log2_max_poc_lsb, start, offset);
		cout << "Poc lsb: " << info.poc_lsb << "\n";
	}
	//ignoring the delta_poc for the moment.
	return true;
}


int Codec::getLength(unsigned char *start, int maxlength, int &duration) {
	if(name == "mp4a") {
		AVFrame *frame = av_frame_alloc();
		if(!frame)
			throw string("Could not create AVFrame");
		AVPacket avp;
		av_init_packet(&avp);

		int got_frame;
		avp.data=(uint8_t *)(start);
		avp.size = maxlength;
		int consumed = avcodec_decode_audio4(context, frame, &got_frame, &avp);

		duration = frame->nb_samples;
		cout << "Duration: " << frame->nb_samples << endl;
		av_freep(&frame);
		return consumed;

	} else if(name == "mp4v") {

		/*     THIS DOES NOT SEEM TO WORK FOR SOME UNKNOWN REASON. IT JUST CONSUMES ALL BYTES.
  *     AVFrame *frame = avcodec_alloc_frame();
		if(!frame)
			throw string("Could not create AVFrame");
		AVPacket avp;
		av_init_packet(&avp);

		int got_frame;
		avp.data=(uint8_t *)(start);
		avp.size = maxlength;
		int consumed = avcodec_decode_video2(context, frame, &got_frame, &avp);
		av_freep(&frame);
		return consumed; */

		//found no way to guess size, probably the only way is to use some functions in ffmpeg
		//to decode the stream
		cout << "Unfortunately I found no way to guess size of mp4v packets. Sorry\n";
		return -1;

	} else if(name == "avc1") {

		static bool contextinited = false;
		static H264sps sps;
		if(!contextinited) {
			H264Context *h = (H264Context *)context->priv_data;//context->codec->
			SPS *hsps = (SPS *)(h->ps.sps_list[0]->data);
			sps.frame_mbs_only_flag = hsps->frame_mbs_only_flag;
			sps.log2_max_frame_num = hsps->log2_max_frame_num;
			sps.log2_max_poc_lsb = hsps->log2_max_poc_lsb;
			sps.poc_type = hsps->poc_type;
		}

		/*		AVFrame *frame = avcodec_alloc_frame();
		if(!frame)
			throw string("Could not create AVFrame");
		AVPacket avp;
		av_init_packet(&avp);

		int got_frame;
		avp.data=(uint8_t *)(start);
		avp.size = maxlength;
		int consumed = avcodec_decode_video2(context, frame, &got_frame, &avp);
		cout << "Consumed: " << consumed << endl;
		av_freep(&frame);
		cout << "Consumed: " << consumed << endl;

		return consumed;
		*/

		/* NAL unit types
		enum {
			NAL_SLICE           = 1, //non keyframe
			NAL_DPA             = 2,
			NAL_DPB             = 3,
			NAL_DPC             = 4,
			NAL_IDR_SLICE       = 5, //keyframe
			NAL_SEI             = 6,
			NAL_SPS             = 7,
			NAL_PPS             = 8,
			NAL_AUD             = 9,
			NAL_END_SEQUENCE    = 10,
			NAL_END_STREAM      = 11,
			NAL_FILLER_DATA     = 12,
			NAL_SPS_EXT         = 13,
			NAL_AUXILIARY_SLICE = 19,
			NAL_FF_IGNORE       = 0xff0f001,
		};
		*/
		//first 4 bytes are the length, then the nal starts.
		//ref_idc !=0 per unit_type = 5
		//ref_idc == 0 per unit_type = 6, 9, 10, 11, 12

		//See 7.4.1.2.4 Detection of the first VCL NAL unit of a primary coded picture
		//for rules on how to group nals into a picture.

		uint32_t length = 0;
		unsigned char *pos = start;

		NalInfo previous;
		bool seen_slice = false;

		while(1) {
			cout << "\n";
			NalInfo info;
			bool ok = getNalInfo(sps, maxlength, pos, info);
			if(!ok) return length;

			switch(info.nal_type) {
			case 1:
			case 5:
				if(!seen_slice) {
					previous = info;
					seen_slice = true;
				} else {
					//check for changes
					if(previous.frame_num != info.frame_num) {
						cout << "Different frame number\n";
						return length;
					}
					if(previous.pps_id != info.pps_id) {
						cout << "Different pps_id\n";
						return length;
					}
					//All these conditions are listed in the docs, but it looks like
					//it creates invalid packets if respected. Puzzling.

					//if(previous.field_pic_flag != info.field_pic_flag) {
					//	cout << "Different field pic flag\n";
					//	return length;
					//}

					//if(previous.bottom_pic_flag != info.bottom_pic_flag) {
					//	cout << "Different bottom pic flag\n";
					//	return length;
					//}
					if(previous.ref_idc != info.ref_idc) {
						cout << "Different ref idc\n";
						return length;
					}
					//if((previous.poc_type == 0  && info.poc_type == 0 && previous.poc_lsb != info.poc_lsb)) {
					//	cout << "Different poc lsb\n";
					//	return length;
					//}
					if(previous.idr_pic_flag != info.idr_pic_flag) {
						cout << "Different nal type (5, 1)\n";
					}
					//if(previous.idr_pic_flag == 1 && info.idr_pic_flag == 1 && previous.idr_pic_id != info.idr_pic_id) {
					//	cout << "Different idr pic id for keyframe\n";
					//	return length;
					//}
				}
				break;
			default:
				if(seen_slice) {
					cerr << "New access unit since seen picture\n";
					return length;
				}
				break;
			}
			pos += info.length;
			length += info.length;
			maxlength -= info.length;
			cout << "Partial length: " << length << "\n";
		}
		return length;

	} else if(name == "samr") { //lenght is multiple of 32, we split packets.
		return 32;
	} else if(name == "twos") { //lenght is multiple of 32, we split packets.
		return 4;
	} else if(name == "apcn") {
		return swap32(*(int *)start);
	} else if(name == "lpcm") {
		// Use hard-coded values for now....
		const int num_samples      = 4096; // Empirical
		const int num_channels     =    2; // Stereo
		const int bytes_per_sample =    2; // 16-bit
		return num_samples * num_channels * bytes_per_sample;
	} else if(name == "in24") {
		return -1;
	} else
		return -1;
}

bool Codec::isKeyframe(unsigned char *start, int maxlength) {
	if(name == "avc1") {
		//first byte of the NAL, the last 5 bits determine type (usually 5 for keyframe, 1 for intra frame)
		return (start[4] & 0x1F) == 5;
	} else
		return false;
}




void Track::parse(Atom *t, Atom *mdat) {

	trak = t;
	Atom *mdhd = t->atomByName("mdhd");
	if(!mdhd)
		throw string("No mdhd atom: unknown duration and timescale");

	timescale = mdhd->readInt(12);
	duration = mdhd->readInt(16);

	times = getSampleTimes(t);
	keyframes = getKeyframes(t);
	sizes = getSampleSizes(t);

	vector<int> chunk_offsets = getChunkOffsets(t);
	vector<int> sample_to_chunk = getSampleToChunk(t, chunk_offsets.size());

	if(times.size() != sizes.size()) {
		cout << "Mismatch between time offsets and size offsets: \n";
		cout << "Time offsets: " << times.size() << " Size offsets: " << sizes.size() << endl;
	}
	//assert(times.size() == sizes.size());
	if(times.size() != sample_to_chunk.size()) {
		cout << "Mismatch between time offsets and sample_to_chunk offsets: \n";
		cout << "Time offsets: " << times.size() << " Chunk offsets: " << sample_to_chunk.size() << endl;
	}
	//compute actual offsets
	int old_chunk = -1;
	int offset = -1;
	for(unsigned int i = 0; i < sizes.size(); i++) {
		int chunk = sample_to_chunk[i];
		int size = sizes[i];
		if(chunk != old_chunk) {
			offset = chunk_offsets[chunk];
			old_chunk= chunk;
		}
		offsets.push_back(offset);
		offset += size;
	}

	//move this stuff into track!
	Atom *hdlr = trak->atomByName("hdlr");
	char type[5];
	hdlr->readChar(type, 8, 4);

	bool audio = (type == string("soun"));

	if(type != string("soun") && type != string("vide"))
		return;

	//move this to Codec

	codec.parse(trak, offsets, mdat);
	codec.codec = avcodec_find_decoder(codec.context->codec_id);
	//if audio use next?

	if(!codec.codec) throw string("No codec found!");
	if(avcodec_open2(codec.context, codec.codec, NULL)<0)
		throw string("Could not open codec: ") + codec.context->codec_name;


	/*
	Atom *mdat = root->atomByName("mdat");
	if(!mdat)
		throw string("Missing data atom");

	//print sizes and offsets
		for(int i = 0; i < 10 && i < sizes.size(); i++) {
		cout << "offset: " << offsets[i] << " size: " << sizes[i] << endl;
		cout << mdat->readInt(offsets[i] - (mdat->start + 8)) << endl;
	} */
}

void Track::writeToAtoms() {
	if(!keyframes.size())
		trak->prune("stss");

	saveSampleTimes();
	saveKeyframes();
	saveSampleSizes();
	saveSampleToChunk();
	saveChunkOffsets();

	Atom *mdhd = trak->atomByName("mdhd");
	mdhd->writeInt(duration, 16);

	//Avc1 codec writes something inside stsd.
	//In particular the picture parameter set (PPS) in avcC (after the sequence parameter set)
	//See http://jaadec.sourceforge.net/specs/ISO_14496-15_AVCFF.pdf for the avcC stuff
	//See http://www.iitk.ac.in/mwn/vaibhav/Vaibhav%20Sharma_files/h.264_standard_document.pdf for the PPS
	//I have found out in shane,banana,bruno and nawfel that pic_init_qp_minus26  is different even for consecutive videos of the same camera.
	//the only thing to do then is to test possible values, to do so remove 28 (the nal header) follow the golomb stuff
	//This test could be done automatically when decoding fails..
	//#define SHANE 6
#ifdef SHANE
	int pps[15] = {
		0x28ee3880,
		0x28ee1620,
		0x28ee1e20,
		0x28ee0988,
		0x28ee0b88,
		0x28ee0d88,
		0x28ee0f88,
		0x28ee0462,
		0x28ee04e2,
		0x28ee0562,
		0x28ee05e2,
		0x28ee0662,
		0x28ee06e2,
		0x28ee0762,
		0x28ee07e2
	};
	if(codec.name == "avc1") {
		Atom *stsd = trak->atomByName("stsd");
		stsd->writeInt(pps[SHANE], 122); //a bit complicated to find.... find avcC... follow first link.
	}
#endif

}

void Track::clear() {
	//times.clear();
	offsets.clear();
	sizes.clear();
	keyframes.clear();
}

void Track::fixTimes() {
	if(codec.name == "samr") {
		times.clear();
		times.resize(offsets.size(), 160);
		return;
	}
	while(times.size() < offsets.size())
		times.insert(times.end(), times.begin(), times.end());
	times.resize(offsets.size());

	duration = 0;
	for(unsigned int i = 0; i < times.size(); i++)
		duration += times[i];
}

vector<int> Track::getSampleTimes(Atom *t) {
	vector<int> sample_times;
	//chunk offsets
	Atom *stts = t->atomByName("stts");
	if(!stts)
		throw string("Missing sample to time atom");

	int entries = stts->readInt(4);
	for(int i = 0; i < entries; i++) {
		int nsamples = stts->readInt(8 + 8*i);
		int time = stts->readInt(12 + 8*i);
		for(int i = 0; i < nsamples; i++)
			sample_times.push_back(time);
	}
	return sample_times;
}

vector<int> Track::getKeyframes(Atom *t) {
	vector<int> sample_key;
	//chunk offsets
	Atom *stss = t->atomByName("stss");
	if(!stss)
		return sample_key;

	int entries = stss->readInt(4);
	for(int i = 0; i < entries; i++)
		sample_key.push_back(stss->readInt(8 + 4*i) - 1);
	return sample_key;
}

vector<int> Track::getSampleSizes(Atom *t) {
	vector<int> sample_sizes;
	//chunk offsets
	Atom *stsz = t->atomByName("stsz");
	if(!stsz)
		throw string("Missing sample to sizeatom");

	int entries = stsz->readInt(8);
	int default_size = stsz->readInt(4);
	if(default_size == 0) {
		for(int i = 0; i < entries; i++)
			sample_sizes.push_back(stsz->readInt(12 + 4*i));
	} else {
		sample_sizes.resize(entries, default_size);
	}
	return sample_sizes;
}



vector<int> Track::getChunkOffsets(Atom *t) {
	vector<int> chunk_offsets;
	//chunk offsets
	Atom *stco = t->atomByName("stco");
	if(stco) {
		int nchunks = stco->readInt(4);
		for(int i = 0; i < nchunks; i++)
			chunk_offsets.push_back(stco->readInt(8 + i*4));

	} else {
		Atom *co64 = t->atomByName("co64");
		if(!co64)
			throw string("Missing chunk offset atom");

		int nchunks = co64->readInt(4);
		for(int i = 0; i < nchunks; i++)
			chunk_offsets.push_back(co64->readInt(12 + i*8));
	}
	return chunk_offsets;
}

vector<int> Track::getSampleToChunk(Atom *t, int nchunks){
	vector<int> sample_to_chunk;

	Atom *stsc = t->atomByName("stsc");
	if(!stsc)
		throw string("Missing sample to chunk atom");

	vector<int> first_chunks;
	int entries = stsc->readInt(4);
	for(int i = 0; i < entries; i++)
		first_chunks.push_back(stsc->readInt(8 + 12*i));
	first_chunks.push_back(nchunks+1);

	for(int i = 0; i < entries; i++) {
		int first_chunk = first_chunks[i];
		int last_chunk = first_chunks[i+1];
		int n_samples = stsc->readInt(12 + 12*i);

		for(int k = first_chunk; k < last_chunk; k++) {
			for(int j = 0; j < n_samples; j++)
				sample_to_chunk.push_back(k-1);
		}
	}

	return sample_to_chunk;
}


void Track::saveSampleTimes() {
	Atom *stts = trak->atomByName("stts");
	assert(stts);
	stts->content.resize(4 + //version
						 4 + //entries
						 8*times.size()); //time table
	stts->writeInt(times.size(), 4);
	for(unsigned int i = 0; i < times.size(); i++) {
		stts->writeInt(1, 8 + 8*i);
		stts->writeInt(times[i], 12 + 8*i);
	}
}

void Track::saveKeyframes() {
	Atom *stss = trak->atomByName("stss");
	if(!stss) return;
	assert(keyframes.size());
	if(!keyframes.size()) return;

	stss->content.resize(4 + //version
						 4 + //entries
						 4*keyframes.size()); //time table
	stss->writeInt(keyframes.size(), 4);
	for(unsigned int i = 0; i < keyframes.size(); i++)
		stss->writeInt(keyframes[i] + 1, 8 + 4*i);
}

void Track::saveSampleSizes() {
	Atom *stsz = trak->atomByName("stsz");
	assert(stsz);
	stsz->content.resize(4 + //version
						 4 + //default size
						 4 + //entries
						 4*sizes.size()); //size table
	stsz->writeInt(0, 4);
	stsz->writeInt(sizes.size(), 8);
	for(unsigned int i = 0; i < sizes.size(); i++) {
		stsz->writeInt(sizes[i], 12 + 4*i);
	}
}

void Track::saveSampleToChunk() {
	Atom *stsc = trak->atomByName("stsc");
	assert(stsc);
	stsc->content.resize(4 + // version
						 4 + //number of entries
						 12); //one sample per chunk.
	stsc->writeInt(1, 4);
	stsc->writeInt(1, 8); //first chunk (1 based)
	stsc->writeInt(1, 12); //one sample per chunk
	stsc->writeInt(1, 16); //id 1 (WHAT IS THIS!)
}

void Track::saveChunkOffsets() {
	Atom *co64 = trak->atomByName("co64");
	if(co64) {
		trak->prune("co64");
		Atom *stbl = trak->atomByName("stbl");
		Atom *new_stco = new Atom;
		memcpy(new_stco->name, "stco", 5);
		stbl->children.push_back(new_stco);
	}
	Atom *stco = trak->atomByName("stco");
	assert(stco);
	stco->content.resize(4 + //version
						 4 + //number of entries
						 4*offsets.size());
	stco->writeInt(offsets.size(), 4);
	for(unsigned int i = 0; i < offsets.size(); i++)
		stco->writeInt(offsets[i], 8 + 4*i);
}


