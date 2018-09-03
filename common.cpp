//==================================================================//
/*                                                                  *
	Untrunc - common.cpp

	Untrunc is GPL software; you can freely distribute,
	redistribute, modify & use under the terms of the GNU General
	Public License; either version 2 or its successor.

	Untrunc is distributed under the GPL "AS IS", without
	any warranty; without the implied warranty of merchantability
	or fitness for either an expressed or implied particular purpose.

	Please see the included GNU General Public License (GPL) for
	your rights and further details; see the file COPYING. If you
	cannot, write to the Free Software Foundation, 59 Temple Place
	Suite 330, Boston, MA 02111-1307, USA.  Or www.fsf.org

	Copyright 2010 Federico Ponchio
	with contributions from others.
 *                                                                  */
//==================================================================//

#include "common.h"

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <limits>

extern "C" {
#include "libavutil/log.h"
}  // extern "C"


// Logging.
LogMode g_log_mode = LogMode::I;

// Configure FFmpeg/Libav logging for use in C++.
#ifdef AV_LOG_PRINT_LEVEL
# define DEFAULT_AVLOG_FLAGS	AV_LOG_PRINT_LEVEL
#else
# define DEFAULT_AVLOG_FLAGS	0
#endif

AvLog::AvLog()          : AvLog(AV_LOG_QUIET, DEFAULT_AVLOG_FLAGS) { }
AvLog::AvLog(int level) : AvLog(level,        DEFAULT_AVLOG_FLAGS) { }

AvLog::AvLog(int level, int flags)
	: level_(av_log_get_level())
#ifdef AV_LOG_PRINT_LEVEL
	, flags_(av_log_get_flags())
#endif
{
	if (level_ < level) av_log_set_level(level);
	av_log_set_flags(flags);
	std::cout.flush();  // Flush C++ standard streams.
	//std::cerr.flush();  // Unbuffered -> nothing to flush.
	std::clog.flush();
}

AvLog::~AvLog() {
	std::fflush(stdout);  // Flush C stdio files.
	std::fflush(stderr);
	av_log_set_level(level_);
#ifdef AV_LOG_PRINT_LEVEL
	av_log_set_flags(flags_);
#endif
}


// Swap the 8-bit bytes into their reverse order.
uint16_t swap16(uint16_t us) {
	return (us >> 8) | (us << 8);
}

uint32_t swap32(uint32_t ui) {
	return ((ui << 24) )             |
		   ((ui <<  8) & 0x00FF0000) |
	       ((ui >>  8) & 0x0000FF00) |
	       ((ui >> 24) );
}

uint64_t swap64(uint64_t ull) {
	return ((ull << 56) )                     |
	       ((ull << 40) & 0x00FF000000000000) |
	       ((ull << 24) & 0x0000FF0000000000) |
	       ((ull <<  8) & 0x000000FF00000000) |
	       ((ull >>  8) & 0x00000000FF000000) |
	       ((ull >> 24) & 0x0000000000FF0000) |
	       ((ull >> 40) & 0x000000000000FF00) |
	       ((ull >> 56) );
}


// Read an unaligned, Golomb encoded value. 
int readGolomb(const uchar** buffer, int* bit_offset) {
	assert(buffer && *buffer && bit_offset);
	assert(unsigned(*bit_offset) < 8);
	if (!buffer || !*buffer || !bit_offset)
		return -1;
	const uchar* buf = *buffer;
	int          ofs = *bit_offset;

	// Count the zeroes.
	int count = 0;
	// Count the leading zeroes.
	while ((*buf & (0x01 << (7 - ofs))) == 0) {
		count++;
		ofs++;
		if (ofs == 8) {
			buf++;
			ofs = 0;
		}
		if (count > 20) {
			std::cerr << "Failed reading golomb: too large!\n";
			*buffer = buf;
			*bit_offset = ofs;
			return -1;
		}
	}
	// Skip the single 1 delimiter.
	ofs++;
	if (ofs == 8) {
		buf++;
		ofs = 0;
	}
	// Read count bits.
	uint32_t res = 1;
	while (count-- > 0) {
		res <<= 1;
		res |= (*buf & (0x01 << (7 - ofs))) >> (7 - ofs);
		ofs++;
		if (ofs == 8) {
			buf++;
			ofs = 0;
		}
	}
	*buffer = buf;
	*bit_offset = ofs;
	return res - 1;
}

// Read bits from an unaligned buffer. 
uint readBits(int numbits, const uchar** buffer, int* bit_offset) {
	assert(buffer && *buffer && bit_offset);
	assert(unsigned(numbits) <= std::numeric_limits<uint>::digits &&
		   unsigned(*bit_offset) < 8);
	const uchar* buf = *buffer;
	int          ofs = *bit_offset;

	uint res = 0;
	int d = 8 - ofs;
	uint mask = ((1 << d) - 1);
	int to_rshift = d - numbits;
	if (to_rshift > 0) {
		res = (*buf & mask) >> to_rshift;
		ofs += numbits;
	} else if (to_rshift == 0) {
		res = (*buf & mask);
		buf++;
		ofs = 0;
	} else {
		res = (*buf & mask);
		numbits -= d;
		buf++;
		ofs = 0;
		while (numbits >= 8) {
			res <<= 8;
			res |= *buf;
			numbits -= 8;
			buf++;
		}
		if (numbits > 0) {
			ofs = numbits;
			res <<= numbits;
			res |= *buf >> (8 - numbits);
		}
	}
	*buffer = buf;
	*bit_offset = ofs;
	return res;
}

#if 0  // Not working correctly.
uint readBits(int n, uchar*& buffer, int& offset) {
	uint res = 0;
	while (n + offset > 8) {  // Can't read in a single reading.
		int d = 8 - offset;
		res <<= d;
		res |= *buffer & ((1 << d) - 1);
		offset = 0;
		buffer++;
		n -= d;
	}
	// Read the remaining bits.
	int d = (8 - offset - n);
	res <<= n;
	res |= (*buffer >> d) & ((1 << n) - 1);
	return res;
}
#endif


// Convert an Atom name to its Id code.
uint32_t name2Id(const std::string& name) {
	// An Atom name is 3 or 4 chars; i.e. the 4th char can be zero.
	if (name.size() >= 3) return name2Id(name.c_str());
	char data[4] = {};
	name.copy(data, 4);
	return name2Id(data);
}

// Convert an Atom Id code to its name.
// Note: Some Atoms may contain non-printable chars (e.g. in user-data).
void id2Name(std::string* name, uint32_t id) {
	name->resize(4);
	id2Name(&((*name)[0]), id);
}


// Is a Terminal/Console/TTY connected to the C++ stream?
// This file MUST be statically linked into the task/process.
namespace {
extern "C" {
#ifdef _WIN32
# include <io.h>        // for: _isatty()
#else
# include <unistd.h>    // for: isatty()
#endif
}  // extern "C"

// Stdio file descriptors.
#ifndef STDIN_FILENO
# define STDIN_FILENO   0
# define STDOUT_FILENO  1
# define STDERR_FILENO  2
#endif

// Store start-up addresses of C++ stdio stream buffers as identifiers.
// These addresses differ per process and must be statically linked in.
// Assume that the stream buffers at these stored addresses
//  are always connected to their underlaying stdio files.
static const std::streambuf* const StdioBufs[] = {
	std::cin.rdbuf(), std::cout.rdbuf(), std::cerr.rdbuf(), std::clog.rdbuf()
};

// Store start-up terminal/TTY statuses of C++ stdio stream buffers.
// These statuses differ per process and must be statically linked in.
// Assume that the statuses don't change during the process life-time.
static const bool StdioTtys[sizeof(StdioBufs)/sizeof(StdioBufs[0])] = {
#ifdef _WIN32
	_isatty(STDIN_FILENO), _isatty(STDOUT_FILENO), _isatty(STDERR_FILENO),
	_isatty(STDERR_FILENO)
#else
	 isatty(STDIN_FILENO),  isatty(STDOUT_FILENO),  isatty(STDERR_FILENO),
	 isatty(STDERR_FILENO)
#endif
};
}  // namespace

// Is a Terminal/Console/TTY connected to the C++ stream?
// Use on C++ stdio chararacter streams: cin, cout, cerr and clog.
bool isATerminal(const std::ios& stream) {
	for (unsigned int i = 0; i < sizeof(StdioBufs)/sizeof(StdioBufs[0]); ++i)
		if(stream.rdbuf() == StdioBufs[i]) return StdioTtys[i];
	return false;
}


// Dump contents of data.
void printBuffer(const uchar* pos, int n) {
	std::cout << std::hex;
	for (int i = 0; i != n; ++i) {
		int x = static_cast<int>(*(pos + i));
		if (x < 16) std::cout << '0';
		std::cout << x << ' ';
	}
	std::cout << std::dec << '\n';
}

namespace {
size_t hexDumpLine(const uchar* p, size_t n, size_t address) {
	if (n == 0) return 0;

	std::ostringstream osstrm;
	osstrm.fill('0');
	osstrm.flags(std::ios_base::hex | std::ios_base::right);

	size_t len = std::min(n, size_t(16));
	osstrm << std::setw(8) << address << ':';
	size_t i = 0;
	for (; i < len; ++i) {
		osstrm << (((i % 4) == 0) ? "  " : " ")
		       << std::setw(2) << static_cast<const unsigned int>(p[i]);
	}
	if (i < 16) osstrm << std::string((16 - i) * 13 / 4, ' ');
	osstrm << "  ";
	for (i = 0; i < len; ++i)
		osstrm << ((std::isprint(p[i])) ? static_cast<char>(p[i]) : '.');
	osstrm << '\n';

	std::cout << osstrm.str();
	return len;
}
};  // namespace

size_t hexDump(const uchar *p, size_t n, size_t address) {
	if (!p) return 0;

	size_t total = 0;
	while (n > 0) {
		size_t len = hexDumpLine(p, n, address);
		total   += len;
		p       += len;
		address += len;
		n       -= len;
	}
	return total;
}

size_t hexDump(const uchar *p, size_t n, const std::string& header,
			   size_t address) {
	if (!header.empty()) {
		std::cout << header;
		if (header.back() != '\n') std::cout << '\n';
	}
	return hexDump(p, n, address);
}


// String conversions.
std::string duration2String(uint64_t duration, uint32_t timescale,
							bool iso_format) {
	if (duration  == 0) return (iso_format) ? "PT0S" : "00:00";
	if (timescale == 0)
		return std::to_string(duration) + " units / <no-timescale>";

	std::ostringstream osstrm;
	osstrm.fill('0');
	osstrm.flags(std::ios_base::dec | std::ios_base::internal);

	uint64_t ms  = duration % timescale;
	uint64_t dur = duration / timescale;
	ms  = ms * 1000 / timescale;
	uint64_t s   = dur % 60;
	dur = dur / 60;
	s   = s * 1000 + ms;  // Seconds in ms.
	unsigned m   = dur % 60;
	uint64_t h   = dur / 60;

	if (iso_format) {
		// ISO 8601 duration notation.
		osstrm << "PT";
		if (h != 0) osstrm << h << 'H';
		if (m != 0) osstrm << m << 'M';
		// When the seconds are not returned, the resulting accuracy is reduced.
		if (s != 0 || (h | m) == 0)
			osstrm << (static_cast<double>(s) / 1000.0) << 'S';
	} else {
		if (h != 0) osstrm << std::setw(2) << h << ':';
		osstrm << std::setw(2) << m
			   << ((s < 10000) ? ":0" : ":")
			   << (static_cast<double>(s) / 1000.0);
	}

	return osstrm.str();
}

std::string duration2String(int64_t duration, uint32_t timescale,
							bool iso_format) {
	const bool sign = (duration < 0);
	return duration2String(static_cast<uint64_t>(std::abs(duration)),
						   timescale, iso_format).insert(0, sign, '-');
}


// vim:set ts=4 sw=4 sts=4 noet:
