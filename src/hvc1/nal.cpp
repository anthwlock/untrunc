#include "nal.h"

#include <iostream>
#include <cassert>
#include <cstdint>

#include "../common.h"

using namespace std;

H265NalInfo::H265NalInfo(const uchar* start, int max_size) {
	is_ok = parseNal(start, max_size);
}

bool h265IsSlice(int nal_type) {
	return
	    nal_type == NAL_TRAIL_N ||
	    nal_type == NAL_TRAIL_R ||
	    nal_type == NAL_CRA_NUT ||
	    nal_type == NAL_RASL_N ||
	    nal_type == NAL_RASL_R ||
//	    nal_type == NAL_IDR_N_LP ||
	    nal_type == NAL_IDR_W_RADL;
}

bool h265IsKeyframe(int nal_type) {
	return
	    nal_type == NAL_IDR_N_LP ||
	    nal_type == NAL_IDR_W_RADL;
}

// see avc1/nal.cpp for more detailed comments
bool H265NalInfo::parseNal(const uchar *buffer, uint32_t maxlength) {

//	cout << "parsing nal here:\n";
//	printBuffer(buffer, 30);

	if(buffer[0] != 0) {
		logg(V, "First byte expected 0\n");
		return false;
	}

	// following only works with 'avcc' bytestream, see avc1/nal.cpp
	uint32_t len = swap32(*(uint32_t *)buffer);
	length_ = len + 4;
	logg(V, "Length: ", length_ - 4, "+4\n");

	if(length_ > maxlength) {
//		cout << "maxlength = " << maxlength << '\n';
//		cout << "length_ = " << length_ << '\n';
		logg(W2, "buffer exceeded by: ", len-maxlength, " | ");
		if (g_log_mode >= W2) printBuffer(buffer, 32);
		return false;
	}
	buffer += 4;
	if(*buffer & (1 << 7)) {
		logg(V, "Warning: Forbidden first bit 1\n");
		is_forbidden_set_ = true;
		// sometimes the length is still correct
		if (!g_ignore_forbidden_nal_bit) return false;
	}
	nal_type_ = *buffer >> 1 ;
	logg(V, "Nal type: ", nal_type_, "\n");

	if(nal_type_ > 40) {
		logg(V, "nal_type_ too big (> 40)\n");
		return false;
	}

	nuh_layer_id_ = (*buffer & 1) << 6 | (*(buffer+1) >> 5);
	logg(V, "nuh_layer_id: ", nuh_layer_id_, "\n");

	nuh_temporal_id_plus1 = (*(buffer+1) & 0b111);
	logg(V, "nuh_temporal_id_plus1: ", nuh_temporal_id_plus1, "\n");

	if ((nal_type_ == NAL_EOB_NUT && nuh_temporal_id_plus1) || (nal_type_ != NAL_EOB_NUT && !nuh_temporal_id_plus1)) {
		logg(V, "Warning: nuh_temporal_id_plus1 is wrong\n");
		return false;
	}

	if (h265IsSlice(nal_type_)) {
		//check size is reasonable:
		if(len < 8) {
			logg(W2, "very short NAL-unit! (len=", len, ", type=", nal_type_, ")\n");
//			return false;
		}

		data_ = buffer+2;
	}
	return true;
}
