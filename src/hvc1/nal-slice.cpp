#include "nal-slice.h"

#include <iostream>

#include "../common.h"
#include "nal.h"

using namespace std;

H265SliceInfo::H265SliceInfo(const H265NalInfo& nal_info) {
	is_ok = decode(nal_info);
}

bool H265SliceInfo::isInNewFrame(const H265SliceInfo& previous_slice __attribute__((unused))) {
	return first_slice_segment_in_pic_flag;
}

bool H265SliceInfo::decode(const H265NalInfo& nal_info) {
	const uchar* start = nal_info.data_;
	first_slice_segment_in_pic_flag = *start >> 7;
	return true;
}
