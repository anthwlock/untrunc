#include "sps-info.h"

#include <iostream>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include <cassert>

#include "../common.h"
#include "nal.h"

using namespace std;

SpsInfo::SpsInfo(const uchar* pos) {
	is_ok = decode(pos);
}

bool SpsInfo::decode(const uchar* pos) {
//	cout << "nal_info.type = " << nal_info.nal_type << '\n';
//	cout << "I am here:\n";
//	printBuffer(pos, 20);
	logg(V, "decoding SPS ...\n");
	int offset = 0;
	pos += 3; // skip 24 bits
	readGolomb(pos, offset); // sps_id

	int log2_max_frame_num_minus4 = readGolomb(pos, offset);
	log2_max_frame_num = log2_max_frame_num_minus4 + 4;
	logg(V, "log2_max_frame_num: ", log2_max_frame_num, '\n');

	poc_type = readGolomb(pos, offset);
	if (poc_type == 0) {
		int log2_max_poc_lsb_minus4 = readGolomb(pos, offset);
		log2_max_poc_lsb = log2_max_poc_lsb_minus4 + 4;
	} else if (poc_type == 1) {
		readBits(1, pos, offset); // delta_pic_order_always_zero_flag
		readGolomb(pos, offset); // offset_for_non_ref_pic
		readGolomb(pos, offset); // offset_for_top_to_bottom_field
		int poc_cycle_length = readGolomb(pos, offset);
		for (int i = 0; i < poc_cycle_length; i++)
			readGolomb(pos, offset); // offset_for_ref_frame[i]
	} else if (poc_type != 2) {
		cout << "invalid poc_type\n";
		return false;
	}

	readGolomb(pos, offset); // ref_frame_count
	readBits(1, pos, offset); // gaps_in_frame_num_allowed_flag
	readGolomb(pos, offset); // mb_width
	readGolomb(pos, offset); // mb_height

	frame_mbs_only_flag = readBits(1, pos, offset);
	return true;
}
