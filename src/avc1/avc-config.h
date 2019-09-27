#ifndef AVCCONFIG_H
#define AVCCONFIG_H

#include "../common.h"

class Atom;
class SpsInfo;

class AvcConfig {
public:
	AvcConfig() = default;
	AvcConfig(const Atom* stsd);
	~AvcConfig();
	bool is_ok = false;
	SpsInfo* sps_info_ = NULL;

private:
	bool decode(const uchar* start);
};

#endif // AVCCONFIG_H
