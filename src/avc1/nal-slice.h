#ifndef NALSLICE_H
#define NALSLICE_H

class SpsInfo;
class NalInfo;

class SliceInfo {
public:
	int first_mb = 0; //unused
	int slice_type = 0;   //should match the nal type (1, 5)
	int pps_id = 0;       //which parameter set to use
	int frame_num = 0;
	int field_pic_flag = 0;
	int bottom_pic_flag = 0; // only if field_pic_flag
	int idr_pic_id = 0;   //read only for nal_type 5
	int poc_type = 0; //if zero check the lsb
	int poc_lsb = 0;

	int idr_pic_flag = 0; //actually 1 for nal_type 5, 0 for nal_type 1
	bool is_ok = false;

	SliceInfo() = default;
	SliceInfo(const NalInfo&, const SpsInfo&);
	bool isInNewFrame(const SliceInfo&);
	bool decode(const NalInfo&, const SpsInfo&);
};

#endif // NALSLICE_H
