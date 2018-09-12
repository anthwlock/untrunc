#ifndef HELPER_H
#define HELPER_H

#include <cstdint>
#include <iostream>
#include <map>

typedef unsigned int uint;
typedef unsigned char uchar;

enum LogMode { E, W, I, V, VV };
extern LogMode g_log_mode;
extern size_t g_max_partsize;
extern bool g_interactive;

template<class... Args>
void logg(Args&&... args){
//	(std::cout << ... << args); // Binary left fold (c++17)
	using creator = int[]; // dummy
	creator{ 0, ( std::cout << (std::forward<Args>(args)), 0) ... };
}

template<class... Args>
void logg(LogMode m, Args&&... x){
	if(g_log_mode < m)
		return;
	if(m == I)
		std::cout << "Info: ";
	else if(m == W)
		std::cout << "Warning: ";
	else if(m == E)
		std::cout << "Error: ";
	logg(std::forward<Args>(x)...);
}

/* NAL unit types */
enum {
	NAL_SLICE           = 1, //non keyframe
	NAL_DPA             = 2,
	NAL_DPB             = 3,
	NAL_DPC             = 4,
	NAL_IDR_SLICE       = 5, //keyframe
	NAL_SEI             = 6,
	NAL_SPS             = 7,
	NAL_PPS             = 8,
	NAL_AUD             = 9,
	NAL_END_SEQUENCE    = 10,
	NAL_END_STREAM      = 11,
	NAL_FILLER_DATA     = 12,
	NAL_SPS_EXT         = 13,
	NAL_AUXILIARY_SLICE = 19,
	NAL_FF_IGNORE       = 0xff0f001,
};



const std::map<std::string, std::string> g_atom_names = {
    {"esds", "ES Descriptor"},
    {"stsd", "sample description"},
    {"minf", "media information"},
    {"stss", "sync samples"},
    {"udta", "user data"},
    {"stsz", "sample to size"},
    {"ctts", "sample to composition time"},
    {"stsc", "sample to chunk"},
    {"stts", "sample to decode time"},
    {"co64", "chunk to offset 64"},
    {"stco", "chunk to offset"}
};

uint16_t swap16(uint16_t us);
uint32_t swap32(uint32_t ui);
uint64_t swap64(uint64_t ull);

int readGolomb(const uchar *&buffer, int &offset);
uint readBits(int n, const uchar *&buffer, int &offset);

void printBuffer(const uchar* pos, int n);
std::string mkHexStr(const uchar* pos, int n, bool bytes_seperated=false);

void hitEnterToContinue();

#endif // HELPER_H
