#ifndef NAL_H
#define NAL_H

#include <cstdint>
#include <vector>

#include "common.h"

class NalInfo {
public:
	NalInfo(const uchar* start, int max_size);

	int length;
	int ref_idc;
	int nal_type;
	NalInfo(): length(0), ref_idc(0), nal_type(0) {}

	bool is_ok;  // did parsing work
	bool is_forbidden_set_;
	uchar* payload_;
	bool parseNal(const uchar* start, uint32_t max_size);
private:
	std::vector<uchar> data_;
};


#endif // NAL_H
