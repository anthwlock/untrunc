#include "common.h"

#include <iostream>
#include <iomanip>  // setprecision
#include <sstream>
#include <cmath>

extern "C" {
#include "libavcodec/avcodec.h"
}
#include "libavutil/ffversion.h"

#include "atom.h"

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
//uint g_max_partsize = 1600000;  // 1.6MB
uint g_max_partsize = 1<<23;  // 8MiB
uint g_max_buf_sz_needed = 1<<19;  // 512kiB
bool g_interactive = true;
bool g_muted = false;
bool g_ignore_unknown = false;
bool g_stretch_video = false;
bool g_show_tracks = false;
bool g_dont_write = false;
bool g_use_chunk_stats = false;
bool g_dont_exclude = false;
bool g_dump_repaired = false;
bool g_search_mdat = false;
bool g_strict_nal_frame_check = true;
bool g_ignore_forbidden_nal_bit = true;
bool g_dont_omit = false;
bool g_noise_buffer_active = false;
bool g_ignore_out_of_bound_chunks = false;
bool g_skip_existing = false;
uint g_num_w2 = 0;
Mp4* g_mp4 = nullptr;
void (*g_onProgress)(int) = nullptr;
void (*g_onStatus)(const string&) = nullptr;
int64_t g_range_start = kRangeUnset;
int64_t g_range_end = kRangeUnset;
std::string g_dst_path;

std::stringstream noise_buffer;
std::streambuf *orig_cout, *orig_cerr;
void enableNoiseBuffer();
void disableNoiseBuffer();

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
	cout << mkHexStr(pos, n, 4) << '\n';
}

string mkHexStr(const uchar* pos, int n, int seperate_each){
	stringstream out;
	out << hex;
	for (int i=0; i != n; ++i) {
		if (seperate_each && i % seperate_each == 0)
			out << (seperate_each? " " : "");

		int x = (int) *(pos+i);
		if (x < 16) out << '0';
		out << x;
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

void outProgress(double now, double all, const string& prefix) {
	double x = round(1000*(now/all));
	if (g_onProgress) g_onProgress(x/10);
	else cout << prefix << x/10 << "%  \r" << flush;
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
	disableNoiseBuffer();
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
	if (g_num_w2 && g_log_mode >= W) {
		cout << string(10, ' ') << '\n';
		cout << g_num_w2 << " warnings were hidden!\n";
	}
	g_num_w2 = 0;
}

void trim_right(string& in) {
	while (in.size() && (isspace(in.back()) || !in.back()))
		in.pop_back();
}


bool contains(const std::initializer_list<string>& c, const std::string& v) {
  return std::find(c.begin(), c.end(), v) != c.end();
}


void HasHeaderAtom::editHeaderAtom(Atom* header_atom, int64_t duration, bool is_tkhd) {
	auto& data = header_atom->content_;
	uint version = data[0];  // version=1 => 64 bit (2x date + 1x duration)

	int bonus = is_tkhd ? 4 : 0;

	if (version == 0 && duration > (1LL << 32)) {
		logg(V, "converting to 64bit version of '", header_atom->name_, "'\n");
		data[0] = 1;
		data.insert(data.begin()+16+bonus, 4, 0x00);
		data.insert(data.begin()+8, 4, 0x00);
		data.insert(data.begin()+4, 4, 0x00);
	}

	if (data[0] == 1)
		header_atom->writeInt64(duration, 24+bonus);
	else
		header_atom->writeInt(duration, 16+bonus);
}

void HasHeaderAtom::editHeaderAtom() {
	editHeaderAtom(header_atom_, duration_);
}

void HasHeaderAtom::readHeaderAtom() {
	auto& data = header_atom_->content_;
	uint version = data[0];  // version=1 => 64 bit (2x date + 1x duration)

	if (version == 1) {
		timescale_ = header_atom_->readInt(20);
		duration_ = header_atom_->readInt64(24);
	} else {
		timescale_ = header_atom_->readInt(12);
		duration_ = header_atom_->readInt(16);
	}
}

int HasHeaderAtom::getDurationInMs() {
	return 1000 * duration_ / timescale_;
}

string getMovExtension(const string& path) {
	auto idx = path.find_last_of(".");
	if (idx == string::npos) return ".mp4";
	auto ext = path.substr(idx);
	if (ext.find("/") != string::npos || ext.find("\\") != string::npos) return ".mp4";
	return ext;
}

// Shannon entropy
double calcEntropy(const vector<uchar>& in) {
   map<char, int> cnt;
   for (char c : in) cnt[c] ++ ;

   double entropy = 0 ;
   for (auto p : cnt) {
	  double freq = (double)p.second / in.size() ;
	  entropy -= freq * log2(freq) ;
   }
   return entropy;
}

int64_t gcd(int64_t a, int64_t b) {
	return b ? gcd(b, a%b) : a;
}

mt19937& getRandomGenerator() {
	static std::random_device rd;
	static std::mt19937 gen(rd());
	return gen;
}

int64_t total_omited = 0;

void cutNoiseBuffer(bool force) {
	if (noise_buffer.tellp() < 1<<16 && !force) return;
	auto s = noise_buffer.str();
	auto off = std::max(0LL, (long long)s.size() - (1<<11));
	s = s.substr(off);
	total_omited += off;
	noise_buffer.str(s);
	noise_buffer.seekp(0, ios::end);
}

void enableNoiseBuffer() {
	orig_cout = std::cout.rdbuf(noise_buffer.rdbuf());
	orig_cerr = std::cerr.rdbuf(noise_buffer.rdbuf());
	g_noise_buffer_active = true;
}

void disableNoiseBuffer() {
	if (!g_noise_buffer_active) return;
	std::cout.rdbuf(orig_cout);
	std::cerr.rdbuf(orig_cerr);
	g_noise_buffer_active = false;

	if (!total_omited) return;

	cutNoiseBuffer(true);
	auto s = noise_buffer.str();
	auto off = s.find_first_of('\n');
	if (off != std::string::npos)
		s = s.substr(off);
	logg("[[ ", total_omited, " bytes omitted, next ", s.size(), " bytes were buffered ]]\n");
	std::cout << s;
//	cout << "---end_buf\n";
	noise_buffer.str("");
	total_omited = 0;
}

void warnIfAlreadyExists(const string& output) {
	if (FileRead::alreadyExists(output)) {
		logg(W, "destination '", output, "' already exists\n");
		hitEnterToContinue();
	}
}

bool isAllZeros(const uchar* buf, int n) {
//	for (int i=0; i < n; i+=4) if (*(int*)(buf+i)) return false;
	for (int i=0; i < n; i++) if (*(buf+i)) return false;
	return true;
}
