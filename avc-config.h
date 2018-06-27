#ifndef AVCCONFIG_H
#define AVCCONFIG_H

#include "common.h"

class Atom;
class SpsInfo;

class AvcConfig
{
public:
	AvcConfig();
	AvcConfig(const Atom& stsd);
	~AvcConfig();
	bool decode(const uchar* start);
	bool is_ok;

	SpsInfo* sps_info_;
};

#endif // AVCCONFIG_H
