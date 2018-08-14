//==================================================================//
/*                                                                  *
	Untrunc - nal-slice.cpp

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

#include "nal-slice.h"

#include "common.h"
#include "nal.h"
#include "sps-info.h"


SliceInfo::SliceInfo(const NalInfo& nal_info, const SpsInfo& sps)
	: first_mb(0),
	  slice_type(0),
	  pps_id(0),
	  frame_num(0),
	  field_pic_flag(0),
	  bottom_pic_flag(0),
	  idr_pic_flag(0),
	  idr_pic_id(0),
	  poc_type(0),
	  poc_lsb(0) {
	is_ok = decode(nal_info, sps);
}

bool SliceInfo::isInNewFrame(const SliceInfo& previous_slice) {
	// See 7.4.1.2.4 Detection of the first VCL NAL unit of a primary coded picture
	// for rules on how to group NALs into a picture.

	// Check for changes.
	if (previous_slice.frame_num != frame_num) {
		logg(V, "Different frame number\n");
		return true;
	}
	if (previous_slice.pps_id != pps_id) {
		logg(W, "Different pps_id\n");
		return true;
	}

	// Different NAL type.
	if (previous_slice.idr_pic_flag != idr_pic_flag) {
		logg(W, "Different nal type (5, 1)\n");
		return true;
	}

	// All these conditions are listed in the docs, but it looks like
	// it creates invalid packets if respected. Puzzling.

	if (previous_slice.field_pic_flag != field_pic_flag) {
		logg(W, "Different field pic flag\n");
		return true;
	}

	if (previous_slice.bottom_pic_flag != bottom_pic_flag &&
		previous_slice.bottom_pic_flag != -1) {
		logg(W, "Different bottom pic flag\n");
		return true;
	}

	// TODO(anthwlock): 'poc_lsb' differs OR 'delta_poc_bottom' differs.
	if (previous_slice.poc_type == 0 && poc_type == 0 &&
		previous_slice.poc_lsb != poc_lsb) {
		logg(W, "Different poc lsb\n");
		return true;
	}
	// TODO(anthwlock): Both 'poc_type' == 1 AND
	//  either 'delta_pic_order_cnt[0]' or 'delta_pic_order_cnt[1]' differ.
	// if (previous_slice.poc_type == 1 && poc_type == 1 && ... )

	if (previous_slice.idr_pic_flag == 1 && idr_pic_flag == 1 &&
		previous_slice.idr_pic_id != idr_pic_id) {
		logg(W, "Different idr pic id for keyframe\n");
		return true;
	}
	return false;
}

bool SliceInfo::decode(const NalInfo& nal_info, const SpsInfo& sps) {
	const uchar* start = nal_info.payload_.data();
	int offset = 0;
	first_mb = readGolomb(start, offset);
	// TODO(ponchio): Is there a max number, so we could validate?
	logg(VV, "First mb: ", first_mb, '\n');

	slice_type = readGolomb(start, offset);
	if (slice_type > 9) {
		logg(W, "Invalid slice type, probably this is not an avc1 sample\n");
		return false;
	}
	pps_id = readGolomb(start, offset);
	logg(VV, "pic paramter set id: ", pps_id, '\n');
	// Pps id: Should be taked from master context (h264_slice.c:1257).

	// Assume separate colour plane flag is 0,
	// otherwise we would have to read colour_plane_id which is 2 bits.

	// Assuming same SPS for all frames.
	frame_num = readBits(sps.log2_max_frame_num, start, offset);
	logg(VV, "Frame num: ", frame_num, '\n');

	// Read 2 flags.
	field_pic_flag = 0;
	bottom_pic_flag = 0;
	if (!sps.frame_mbs_only_flag) {
		field_pic_flag = readBits(1, start, offset);
		if (field_pic_flag) {
			bottom_pic_flag = readBits(1, start, offset);
		}
	}
	idr_pic_flag = (nal_info.nal_type_ == NAL_IDR_SLICE) ? 1 : 0;
	if (nal_info.nal_type_ == NAL_IDR_SLICE) {
		idr_pic_id = readGolomb(start, offset);
	}

	// If pic order cnt type == 0.
	if (sps.poc_type == 0) {
		poc_lsb = readBits(sps.log2_max_poc_lsb, start, offset);
		logg(VV, "Poc lsb: ", poc_lsb, '\n');
	}
	// TODO(ponchio): Ignoring the delta_poc for the moment.
	return true;
}


// vim:set ts=4 sw=4 sts=4 noet:
