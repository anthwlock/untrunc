#ifndef NAL_H
#define NAL_H

#include "common.h"

class NalInfo {
public:
	NalInfo() = default;
	NalInfo(const uchar* start, int max_size);

	uint length_ = 0;
	int ref_idc_ = 0;
	int nal_type_ = 0;

	bool is_ok;  // did parsing work
	bool is_forbidden_set_;
	const uchar* data_;
	bool parseNal(const uchar* start, uint32_t max_size);
};


#endif // NAL_H
