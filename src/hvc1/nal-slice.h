#ifndef H265_NALSLICE_H
#define H265_NALSLICE_H

class SpsInfo;
class H265NalInfo;

class H265SliceInfo {
public:
	bool first_slice_segment_in_pic_flag = true;

	bool is_ok = false;

	H265SliceInfo() = default;
	H265SliceInfo(const H265NalInfo&);
	bool isInNewFrame();
	bool decode(const H265NalInfo&);
};

#endif // H265_NALSLICE_H
