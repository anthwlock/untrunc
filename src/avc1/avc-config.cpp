#include "avc-config.h"

#include "iostream"

#include "sps-info.h"
#include "../common.h"
#include "../atom.h"

using namespace std;

AvcConfig::AvcConfig(const Atom* stsd) {
	// find avcC payload
	const uchar* start = stsd->content_.data()+12;
	char pattern[5] = "avcC";
	int found = 0;
	int limit = stsd->length_-16;
	while (limit--){
		if (*start++ == pattern[found])
			found++;
		else if (found)
			found = 0;
		if(found == 4)
			break;
	}
	if (found != 4) {
		logg(V, "avcC signature not found\n");
		is_ok = false;
		return;
	}
	int off = start - stsd->content_.data();
	int len = stsd->length_ - off;
	logg(V, "found avcC after: ", off, '\n');
	logg(V, "remaining len:", len, '\n');

	is_ok = decode(start);
}

AvcConfig::~AvcConfig() {
	delete sps_info_;
}

bool AvcConfig::decode(const uchar* start) {
	logg(V, "parsing avcC ...\n");
	int off = 0;
	int ver = readBits(8, start, off); // config_version
	if (ver != 1){
		logg(V, "avcC config version != 1\n");
		return false;
	}
	start += 4;
	uint reserved = readBits(3, start, off); // 111
	if (reserved != 7){
		logg(V, "avcC - reserved is not reserved: ", reserved, '\n');
		return false;
	}
	uint num_sps = readBits(5, start, off);
	if (num_sps != 1)
		logg(W, "avcC contains more than 1 SPS");
	uint len_sps = readBits(16, start, off);
	logg(V, "len_sps: ", len_sps, '\n');
	sps_info_ = new SpsInfo(start);
	return sps_info_->is_ok;
}
