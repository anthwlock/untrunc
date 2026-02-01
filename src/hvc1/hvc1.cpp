#include "hvc1.h"

#include <iostream>

#include "../codec.h"
#include "../mp4.h"

#include "nal-slice.h"
#include "nal.h"

using namespace std;

struct LenResult {
	vector<int> alternative_lengths;
	int length = 0;
};

static
LenResult getLengths(Codec* self, const uchar* start, uint maxlength) {
	LenResult r;
	int& length = r.length;
	const uchar *pos = start;

//	H265SliceInfo previous_slice;
	bool seen_slice = false;
	H265NalInfo previous_nal;
	self->was_keyframe_ = false;


	while(1) {
		logg(V, "---\n");
		logg(V, "pos: ", g_mp4->offToStr(self->cur_off_ + length), "\n");
		H265NalInfo nal_info(pos, maxlength);
		if(!nal_info.is_ok){
			logg(V, "failed parsing h256 nal-header\n");
			return r;
		}

		if (h265IsKeyframe(nal_info.nal_type_)) self->was_keyframe_ = true;
		if (h265IsSlice(nal_info.nal_type_)) {
			H265SliceInfo slice_info(nal_info);
			if (previous_nal.is_ok) {
				if (seen_slice && slice_info.isInNewFrame()) return r;
				if (previous_nal.nuh_layer_id_ != nal_info.nuh_layer_id_){
					logg(W, "Different nuh_layer_id_ idc\n");
					return r;
				}
				if (previous_nal.nuh_temporal_id_plus1 != nal_info.nuh_temporal_id_plus1){
					logg(W, "Different nuh_temporal_id_plus1 idc\n");
					return r;
				}
			}
			seen_slice = true;
		}
		else switch(nal_info.nal_type_) {
		case NAL_AUD: // Access unit delimiter
			if (!previous_nal.is_ok)
				break;
			return r;
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
				return r;
			}
			break;
		}

		if (self->ss_stats_->wouldExceed("hvc1", length, nal_info.length_, g_allow_large_sample ? 1 : self->was_keyframe_)) {
			return r;
		}
		if (self->ss_stats_->isBigEnough(length, self->was_keyframe_)) {
			r.alternative_lengths.push_back(length);
		}

		pos += nal_info.length_;
		length += nal_info.length_;
		maxlength -= nal_info.length_;
		if (maxlength == 0) // we made it
			return r;

		pos = self->loadAfter(length);
		previous_nal = move(nal_info);
		logg(V, "Partial hvc1-length: ", length, "\n");
	}
	return r;
}


int getSizeHvc1(Codec* self, const uchar* start, uint maxlength) {
	auto r = getLengths(self, start, maxlength);
	if (r.alternative_lengths.size()) {
		auto& lens = r.alternative_lengths;
		lens.push_back(r.length);
		return g_mp4->findSizeWithContinuation(self->cur_off_, lens);
	}
	return r.length;
}
