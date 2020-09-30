#include "avc1.h"

#include <iostream>

#include "../codec.h"
#include "../mp4.h"

#include "sps-info.h"
#include "avc-config.h"
#include "nal-slice.h"
#include "nal.h"

using namespace std;

int getSizeAvc1(Codec* self, const uchar* start, uint maxlength) {
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
		if (self->avc_config_->is_ok){
			sps_info = *self->avc_config_->sps_info_;
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
	self->was_keyframe_ = false;

	while(1) {
		logg(V, "---\n");
		if (self->chk_for_twos_ && Codec::looksLikeTwosOrSowt(pos)) return length;
		NalInfo nal_info(pos, maxlength);
		bool was_keyframe = false;
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
			was_keyframe = true;
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
			self->was_keyframe_ = self->was_keyframe_ || was_keyframe;
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
				logg(W2, "New access unit since seen picture (type: ", nal_info.nal_type_, ")\n");
				return length;
			}
			break;
		}
		pos += nal_info.length_;
		length += nal_info.length_;
		maxlength -= nal_info.length_;
		if (maxlength == 0) // we made it
			return length;
		pos = self->loadAfter(length);
		logg(V, "Partial avc1-length: ", length, "\n");
	}
	return length;
}

