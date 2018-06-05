#include "nal.h"

//AVC1

#include <cstdint>
#include <iostream>
#include <vector>

#include <cassert>

#include "common.h"

using namespace std;


NalInfo::NalInfo(const uchar* start, int max_size) {
	is_ok = parseNal(start, max_size);
}

//return false means this probably is not a nal.
bool NalInfo::parseNal(const uchar *buffer, uint32_t maxlength) {

//	cout << "parsing nal here:\n";
//	printBuffer(buffer, 30);

	if(buffer[0] != 0) {
		logg(V, "First byte expected 0\n");
		return false;
	}
	//this is supposed to be the length of the NAL unit.
	uint32_t len = swap32(*(uint32_t *)buffer);
	length = len + 4;
	logg(V, "Length: ", length, "\n");


	int MAX_AVC1_LENGTH = 8*(1<<20); // 8MB
	if(len > MAX_AVC1_LENGTH) {
		logg(V, "Max length exceeded\n");
		return false;
	}

	if(length > maxlength) {
		cout << "maxlength = " << maxlength << '\n';
		cout << "len - maxlength = " << len - maxlength << '\n';
		cout << "Buffer size exceeded\n";
		return false;
	}
	buffer += 4;
	if(*buffer & (1 << 7)) {
		logg(V, "Warning: Forbidden first bit 1\n");
		is_forbidden_set_ = true;
		// means payload is garbage, header is ok though
		// dont return false
	}
	ref_idc = *buffer >> 5;
	logg(V, "Ref idc: ", ref_idc, "\n");

	nal_type = *buffer & 0x1f;
	logg(V, "Nal type: ", nal_type, "\n");
	if(nal_type != NAL_SLICE
	   && nal_type != NAL_IDR_SLICE
	   && nal_type != NAL_SPS)
		return true;

	//check size is reasonable:
	if(len < 8) {
		cout << "Too short!\n";
		return false;
	}

	buffer++; //skip nal header

	//remove the emulation prevention 3 byte.
	//could be done in place to speed up things.
	data_.reserve(len);
	for(int i =0; i < len; i++) {
		if(i+2 < len && buffer[i] == 0 && buffer[i+1] == 0 && buffer[i+2] == 3) {
			data_.push_back(buffer[i]);
			data_.push_back(buffer[i+1]);
			assert(buffer[i+2] == 0x3);
			i += 2; //skipping 0x3 byte!
		} else
			data_.push_back(buffer[i]);
	}

	payload_ = data_.data();
	return true;
}
