#ifndef HELPER_H
#define HELPER_H

#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <random>

class Mp4;

using uint = unsigned int;
using uchar = unsigned char;

using buffs_t = std::vector<std::vector<uchar>>;
using offs_t = std::vector<off_t>;

#define to_uint(a) static_cast<unsigned int>(a)
#define to_size_t(a) static_cast<size_t>(a)

enum LogMode { ET, E, W, I, W2, V, VV };
extern LogMode g_log_mode;
extern uint
    g_max_partsize,       // max theoretical part size
    g_max_buf_sz_needed;  // for determining part size
extern bool g_interactive, g_muted, g_ignore_unknown, g_stretch_video, g_show_tracks,
    g_dont_write, g_use_chunk_stats, g_dont_exclude, g_dump_repaired, g_search_mdat,
    g_strict_nal_frame_check, g_ignore_forbidden_nal_bit, g_noise_buffer_active, g_dont_omit,
    g_ignore_out_of_bound_chunks, g_skip_existing;
extern int64_t g_range_start, g_range_end;
extern std::string g_dst_path;

extern const bool is_new_ffmpeg_api;
extern std::string g_version_str;
extern uint g_num_w2;  // hidden warnings
extern Mp4* g_mp4;
extern void (*g_onProgress)(int);
extern void (*g_onStatus)(const std::string&);

const int64_t kRangeUnset = std::numeric_limits<int64_t>::min();

#define UNFOLD_PARAM_PACK(pack, os) \
	_Pragma("GCC diagnostic push"); \
	_Pragma("GCC diagnostic ignored \"-Wunused-value\""); \
	using creator = int[]; \
	creator{ 0, ( os << (std::forward<decltype(pack)>(pack)), 0) ... }; \
	_Pragma("GCC diagnostic pop"); \

void cutNoiseBuffer(bool force=false);

template<class... Args>
std::string ss(Args&&... args){
	std::stringstream ss;
	UNFOLD_PARAM_PACK(args, ss)
	return ss.str();
}

template<class... Args>
void logg(Args&&... args){
//	(std::cout << ... << args); // Binary left fold (c++17)
	UNFOLD_PARAM_PACK(args, std::cout);

	if (g_noise_buffer_active) cutNoiseBuffer();
}

template<class... Args>
void logg(LogMode m, Args&&... x){
	if (g_log_mode < m) {
		if (m == W2) g_num_w2++;
		return;
	}
	if (m == I)
		std::cout << "Info: ";
	else if (m == W || m == W2)
		std::cout << "Warning: ";
	else if (m <= E) {
		std::cout << "Error: ";
		if (m == ET) {
			logg(std::forward<Args>(x)...);
			if (g_onProgress) throw std::runtime_error(ss(std::forward<Args>(x)...));
			else exit(1);
		}
	}
	logg(std::forward<Args>(x)...);
}

bool contains(const std::initializer_list<std::string>& c, const std::string& v);

template <typename Container>
bool contains(Container const& c, typename Container::const_reference v) {
  return std::find(c.begin(), c.end(), v) != c.end();
}

std::mt19937& getRandomGenerator();

template <template <typename, typename...> class C, class T>
C<T> choose100(const C<T>& in) {
	size_t n = 100, k = 10;
	if (n > in.size()) return in;

	auto gen = getRandomGenerator();
	std::uniform_int_distribution<size_t> dis(0, std::distance(in.begin(), in.end()) - 1 - k);
	C<T> out;

	for (uint i=0; i < k-1; i++) {
		size_t idx = dis(gen);
		for (size_t j=idx; j < idx+k; j++)
			out.push_back(in[j]);
	}

	auto it = in.end() - k;
	for (uint i=0; i < k; i++) out.push_back(*it++);

	std::sort(out.begin(), out.end());
	return out;
}


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
    {"stco", "chunk to offset"},
    {"mvhd", "movie header"},
    {"mdhd", "media header"}
};

void mute();
void unmute();
void enableNoiseBuffer();
void disableNoiseBuffer();

uint16_t swap16(uint16_t us);
uint32_t swap32(uint32_t ui);
uint64_t swap64(uint64_t ull);

void outProgress(double now, double all, const std::string& prefix="");

int readGolomb(const uchar *&buffer, int &offset);
uint readBits(int n, const uchar *&buffer, int &offset);

void printBuffer(const uchar* pos, int n);
std::string mkHexStr(const uchar* pos, int n, int seperate_each=0);

void hitEnterToContinue(bool new_line=true);

std::string pretty_bytes(double bytes);
void printVersion();

void chkHiddenWarnings();

void trim_right(std::string& in);

std::string getMovExtension(const std::string& path);
std::string getOutputSuffix();

double calcEntropy(const std::vector<uchar>& in);
int64_t gcd(int64_t a, int64_t b);

void warnIfAlreadyExists(const std::string&);
bool isAllZeros(const uchar* buf, int n);

class Atom;
// this class is meant for reading/writing mvhd and mdhd
class HasHeaderAtom {
public:
	int timescale_;
	int64_t duration_;

	static void editHeaderAtom(Atom* header_atom, int64_t duration, bool is_tkhd=false);
	void editHeaderAtom();
	void readHeaderAtom();
	int getDurationInMs();

protected:
	Atom* header_atom_;
};

#endif // HELPER_H
