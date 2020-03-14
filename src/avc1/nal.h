#ifndef NAL_H
#define NAL_H

#include "../common.h"

/* H264 NAL unit types */
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


class NalInfo {
public:
	NalInfo() = default;
	NalInfo(const uchar* start, int max_size);

	uint length_ = 0;
	int ref_idc_ = 0;
	int nal_type_ = 0;

	bool is_ok = false;  // did parsing work
	bool is_forbidden_set_ = false;
	const uchar* data_ = nullptr;
	bool parseNal(const uchar* start, uint32_t max_size);
};


#endif // NAL_H
