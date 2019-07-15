#include "common.h"

#include <iostream>
#include <iomanip>  // setprecision
#include <sstream>
#include <cmath>

extern "C" {
#include "libavcodec/avcodec.h"
}
#include "libavutil/ffversion.h"

#ifndef UNTR_VERSION
#define UNTR_VERSION "?"
#endif

// http://git.ffmpeg.org/gitweb/ffmpeg.git/commit/061a0c14bb57
#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 107, 100))
const bool is_new_ffmpeg_api = false;
#else
const bool is_new_ffmpeg_api = true;
#endif

using namespace std;

LogMode g_log_mode = LogMode::I;
size_t g_max_partsize = 1600000;
bool g_interactive = true;
bool g_muted = false;
bool g_ignore_unknown = false;
bool g_stretch_video = false;
bool g_show_tracks = false;
bool g_dont_write = false;
uint g_num_w2 = 0;
Mp4* g_mp4 = nullptr;

std::string g_version_str = "version '" UNTR_VERSION "' using ffmpeg '" FFMPEG_VERSION "'";

uint16_t swap16(uint16_t us) {
	return (us >> 8) | (us << 8);
}

uint32_t swap32(uint32_t ui) {
	return (ui >> 24) | ((ui<<8) & 0x00FF0000) | ((ui>>8) & 0x0000FF00) | (ui << 24);
}

uint64_t swap64(uint64_t ull) {
	return (ull >> 56) |
	        ((ull<<40) & 0x00FF000000000000) |
	        ((ull<<24) & 0x0000FF0000000000) |
	        ((ull<<8)  & 0x000000FF00000000) |
	        ((ull>>8)  & 0x00000000FF000000) |
	        ((ull>>24) & 0x0000000000FF0000) |
	        ((ull>>40) & 0x000000000000FF00) |
	        (ull << 56);
}

int readGolomb(const uchar *&buffer, int &offset) {
	//count the zeroes;
	int count = 0;
	//count the leading zeroes
	while((*buffer & (0x1<<(7 - offset))) == 0) {
		count++;
		offset++;
		if(offset == 8) {
			buffer++;
			offset = 0;
		}
		if(count > 20) {
			cout << "Failed reading golomb: too large!\n";
			return -1;
		}
	}
	//skip the single 1 delimiter
	offset++;
	if(offset == 8) {
		buffer++;
		offset = 0;
	}
	uint32_t res = 1;
	//read count bits
	while(count-- > 0) {
		res <<= 1;
		res |= (*buffer  & (0x1<<(7 - offset))) >> (7 - offset);
		offset++;
		if(offset == 8) {
			buffer++;
			offset = 0;
		}
	}
	return res-1;
}

void printBuffer(const uchar* pos, int n){
	cout << mkHexStr(pos, n, true) << '\n';
}

string mkHexStr(const uchar* pos, int n, bool bytes_seperated){
	stringstream out;
	out << hex;
	for (int i=0; i != n; ++i) {
		int x = (int) *(pos+i);
		if (x < 16) out << '0';
		out << x << (bytes_seperated?" ":"");
	}
	return out.str();
}

uint readBits(int n, const uchar *&buffer, int &offset) {
	uint res = 0;
	int d = 8 - offset;
	uint mask = ((1 << d)-1);
	int to_rshift = d - n;
	if (to_rshift > 0){
		res = (*buffer & mask) >> to_rshift;
		offset += n;
	} else if (to_rshift == 0){
		res = (*buffer & mask);
		buffer++;
		offset = 0;
	} else {
		res = (*buffer & mask);
		n -= d;
		buffer++;
		offset = 0;
		while (n >= 8){
			res <<= 8;
			res |= *buffer;
			n -= 8;
			buffer++;
		}
		if(n > 0){
			offset = n;
			res <<= n;
			res |= *buffer >> (8-n);
		}
	}
	return res;
}


// not working correctly
//uint readBits(int n, uchar *&buffer, int &offset) {
//    int res = 0;
//    while(n + offset > 8) { //can't read in a single reading
//        int d = 8 - offset;
//        res <<= d;
//        res |= *buffer & ((1<<d) - 1);
//        offset = 0;
//        buffer++;
//        n -= d;
//    }
//    //read the remaining bits
//    int d = (8 - offset - n);
//    res <<= n;
//    res |= (*buffer >> d) & ((1 << n) - 1);
//    return res;
//}


void hitEnterToContinue(bool new_line) {
	if (g_interactive) {
		cout << "  [[Hit enter to continue]]" << (new_line? "\n" : "") << flush;
		getchar();
	}
//	else cout << '\n';
}

void outProgress(double now, double all) {
	double x = round(1000*(now/all));
	cout << x/10 << "%  \r" << flush;
}

void mute() {
	g_muted = true;
	av_log_set_level(AV_LOG_QUIET);
}

void unmute() {
	g_muted = false;
	if(g_log_mode <= E) av_log_set_level(AV_LOG_QUIET);
	else if(g_log_mode < V) av_log_set_level(AV_LOG_WARNING);
	else if(g_log_mode > V) av_log_set_level(AV_LOG_DEBUG);
}

string pretty_bytes(double num) {
	uint idx = 0;
	vector<string> units = {"","Ki","Mi","Gi","Ti","Pi","Ei","Zi"};
	while (idx+1 < units.size()) {
		if (num < 1024) break;
		num /= 1024;
		idx++;
	}
	stringstream s;
	s << setprecision(3) << num << units[idx] << "B";
	return s.str();
}

void chkHiddenWarnings() {
	if (g_num_w2) {
		cout << string(10, ' ') << '\n';
		logg(W, g_num_w2, " warnings were hidden!\n");
	}
}

string trim_right(string& in) {
	int idx = in.size()-1;
	for (; in[idx] == ' '; idx--);
	return in.substr(0, idx+1);
}


bool contains(const std::initializer_list<string>& c, const std::string& v) {
  return std::find(c.begin(), c.end(), v) != c.end();
}
