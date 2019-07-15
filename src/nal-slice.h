#ifndef NALSLICE_H
#define NALSLICE_H

#include <cstdint>

class SpsInfo;
class NalInfo;

class SliceInfo {
public:
	int first_mb; //unused
	int slice_type;   //should match the nal type (1, 5)
	int pps_id;       //which parameter set to use
	int frame_num;
	int field_pic_flag;
	int bottom_pic_flag; // only if field_pic_flag
	int idr_pic_id;   //read only for nal_type 5
	int poc_type; //if zero check the lsb
	int poc_lsb;

	int idr_pic_flag; //actually 1 for nal_type 5, 0 for nal_type 1
	bool is_ok;

	SliceInfo(const NalInfo&, const SpsInfo&);
	SliceInfo() : is_ok(false) {};
	bool isInNewFrame(const SliceInfo&);
	bool decode(const NalInfo&, const SpsInfo&);
};

#endif // NALSLICE_H
