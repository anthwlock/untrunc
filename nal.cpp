#include "nal.h"

#include <iostream>
#include <cassert>
#include <cstdint>

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
	// FIXIT: only true if 'avcc' bytestream standard used, not 'Annex B'!
	uint32_t len = swap32(*(uint32_t *)buffer);
	length_ = len + 4;
	logg(V, "Length: ", length_, "\n");


	uint MAX_AVC1_LENGTH = 8*(1<<20); // 8MB
	if(len > MAX_AVC1_LENGTH) {
		logg(V, "Max length exceeded\n");
		return false;
	}

	if(length_ > maxlength) {
//		cout << "maxlength = " << maxlength << '\n';
//		cout << "length_ = " << length_ << '\n';
		logg(W, "buffer exceeded by: ", len-maxlength, '\n');
		return false;
	}
	buffer += 4;
	if(*buffer & (1 << 7)) {
		logg(V, "Warning: Forbidden first bit 1\n");
		is_forbidden_set_ = true;
		// means payload is garbage, header might be ok though
		// so dont return false
	}
	ref_idc_ = *buffer >> 5;
	logg(V, "Ref idc: ", ref_idc_, "\n");

	nal_type_ = *buffer & 0x1f;
	logg(V, "Nal type: ", nal_type_, "\n");
	if(nal_type_ != NAL_SLICE
	   && nal_type_ != NAL_IDR_SLICE
	   && nal_type_ != NAL_SPS)
		return true;

	//check size is reasonable:
	if(len < 8) {
		cout << "Too short!\n";
		return false;
	}

	buffer++; //skip nal header

	// remove the emulation prevention 3 byte.
	// could be done in place to speed up things.
	// EDIT: only needed for 'annex b' bytestream standard, which
	//       is currently not supported anyways. See nal-decoder.
	// FIXIT: citation needed

//	data_.reserve(len);
//	for(int i =0; i < len; i++) {
//		if(i+2 < len && buffer[i] == 0 && buffer[i+1] == 0 && buffer[i+2] == 3) {
//			data_.push_back(buffer[i]);
//			data_.push_back(buffer[i+1]);
//			assert(buffer[i+2] == 0x3);
//			i += 2; //skipping 0x3 byte!
//		} else
//			data_.push_back(buffer[i]);
//	}
	data_ = buffer;
	return true;
}
