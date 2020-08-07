#ifndef H265_NAL_H
#define H265_NAL_H

#include "../common.h"

/* H265 NAL unit types */
enum {
	NAL_TRAIL_N     = 0, // non keyframe
	NAL_TRAIL_R     = 1,
	NAL_RASL_N      = 8,
	NAL_RASL_R      = 9,
	NAL_IDR_W_RADL  = 19, // keyframe
	NAL_IDR_N_LP    = 20,
	NAL_CRA_NUT     = 21,
	NAL_AUD         = 35,  // Access unit delimiter
	NAL_EOB_NUT     = 37,  // End of bitstream
	NAL_FILLER_DATA = 38,
};


class H265NalInfo {
public:
	H265NalInfo() = default;
	H265NalInfo(const uchar* start, int max_size);

	uint length_ = 0;
	int nuh_layer_id_ = 0;
	int nal_type_ = 0;
	int nuh_temporal_id_plus1 = 0;

	bool is_ok = false;  // did parsing work
	bool is_forbidden_set_ = false;
	const uchar* data_ = nullptr;
	bool parseNal(const uchar* start, uint32_t max_size);
};

bool h265IsSlice(int nal_type);
bool h265IsKeyframe(int nal_type);

#endif // H265_NAL_H
