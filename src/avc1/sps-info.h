#ifndef SPSINFO_H
#define SPSINFO_H

#include "../common.h"

class NalInfo;

class SpsInfo {
public:
	SpsInfo() = default;
	SpsInfo(const uchar* pos);

	// default values in case SPS is not decoded yet...
	int log2_max_frame_num = 4;
	bool frame_mbs_only_flag = true;
	int poc_type = 0;
	int log2_max_poc_lsb = 5;

	bool is_ok = false;
	bool decode(const uchar* pos);
};

#endif // SPSINFO_H
