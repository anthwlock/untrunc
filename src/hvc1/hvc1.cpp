#include "hvc1.h"

#include <iostream>

#include "../codec.h"
#include "../mp4.h"

#include "nal-slice.h"
#include "nal.h"

using namespace std;

int getSizeHvc1(Codec* self, const uchar* start, uint maxlength) {
	uint32_t length = 0;
	const uchar *pos = start;

//	H265SliceInfo previous_slice;
	H265NalInfo previous_nal;
	self->was_keyframe_ = false;

	while(1) {
		logg(V, "---\n");
		logg(V, "pos: ", g_mp4->offToStr(self->cur_off_ + length), "\n");
		H265NalInfo nal_info(pos, maxlength);
		if(!nal_info.is_ok){
			logg(V, "failed parsing h256 nal-header\n");
			return length;
		}

		if (h265IsKeyframe(nal_info.nal_type_)) self->was_keyframe_ = true;
		if (h265IsSlice(nal_info.nal_type_)) {
			H265SliceInfo slice_info(nal_info);
			if (previous_nal.is_ok) {
				if (slice_info.isInNewFrame()) return length;
				if (previous_nal.nuh_layer_id_ != nal_info.nuh_layer_id_){
					logg(W, "Different nuh_layer_id_ idc\n");
					return length;
				}
			}
		}
		else switch(nal_info.nal_type_) {
		case NAL_AUD: // Access unit delimiter
			if (!previous_nal.is_ok)
				break;
			return length;
		case NAL_FILLER_DATA:
			if (g_log_mode >= V) {
				logg(V, "found filler data: ");
				printBuffer(pos, 30);
			}
			break;
		default:
			vector<int> dont_warn = {20, 32, 33, 34, 39};
			if (!contains(dont_warn, nal_info.nal_type_))
				logg(W2, "unhandled nal_type: ", nal_info.nal_type_, "\n");
			if (nal_info.is_forbidden_set_) {
				logg(W2, "got forbidden bit.. ", nal_info.nal_type_, ")\n");
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
		previous_nal = move(nal_info);
		logg(V, "Partial hvc1-length: ", length, "\n");
	}
	return length;
}

