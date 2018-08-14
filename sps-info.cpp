//==================================================================//
/*                                                                  *
	Untrunc - sps-info.cpp

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
	with contributions from others.
 *                                                                  */
//==================================================================//

#include "sps-info.h"

#include <cassert>
#include <iostream>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}
#include "common.h"
#include "nal.h"


using std::cout;


SpsInfo::SpsInfo(const uchar* pos) {
	is_ok = decode(pos);
}

bool SpsInfo::decode(const uchar* pos) {
#if 0
	const uchar* pos = nal_info.payload_.data();
	cout << "nal_info.type = " << nal_info.nal_type << '\n';
	cout << "I am here:\n";
	printBuffer(pos, 20);
#endif
	logg(V, "decoding SPS ...\n");
	int offset = 0;
	pos += 3;                 // Skip 24 bits.
	readGolomb(pos, offset);  // sps_id.

	int log2_max_frame_num_minus4 = readGolomb(pos, offset);
	log2_max_frame_num = log2_max_frame_num_minus4 + 4;
	logg(V, "log2_max_frame_num: ", log2_max_frame_num, '\n');

	poc_type = readGolomb(pos, offset);
	if (poc_type == 0) {
		int log2_max_poc_lsb_minus4 = readGolomb(pos, offset);
		log2_max_poc_lsb = log2_max_poc_lsb_minus4 + 4;
	} else if (poc_type == 1) {
		readBits(1, pos, offset);     // delta_pic_order_always_zero_flag.
		readGolomb(pos, offset);      // offset_for_non_ref_pic.
		readGolomb(pos, offset);      // offset_for_top_to_bottom_field.
		int poc_cycle_length = readGolomb(pos, offset);
		for (int i = 0; i < poc_cycle_length; i++)
			readGolomb(pos, offset);  // offset_for_ref_frame[i].
	} else if (poc_type != 2) {
		cout << "invalid poc_type\n";
		return false;
	}

	readGolomb(pos, offset);   // ref_frame_count.
	readBits(1, pos, offset);  // gaps_in_frame_num_allowed_flag.
	readGolomb(pos, offset);   // mb_width.
	readGolomb(pos, offset);   // mb_height.

	frame_mbs_only_flag = readBits(1, pos, offset);
	return true;
}


// vim:set ts=4 sw=4 sts=4 noet:
