#ifndef H264SPS_H
#define H264SPS_H

#include "common.h"

#include <cstdint>

class NalInfo;

class SpsInfo {
public:
	SpsInfo();
	SpsInfo(const uchar* pos);
	int log2_max_frame_num;
	bool frame_mbs_only_flag;
	int poc_type;
	int log2_max_poc_lsb;

	bool is_ok;
	bool decode(const uchar* pos);
};

#endif // H264SPS_H
