//==================================================================//
/*                                                                  *
	Untrunc - common.h

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

#ifndef COMMON_H_
#define COMMON_H_

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>


using uint = unsigned int;
using uchar = unsigned char;


// Width in spaces of an indentation step.
const int kIndentStep = 2;


extern size_t g_max_partsize;


enum LogMode { E, W, I, V, VV };
extern LogMode g_log_mode;

template <class... Args>
void logg(Args&&... args) {
	//(std::cout << ... << args);  // Binary left fold (C++17).
	using creator = int[];  // Dummy.
	creator{0, (std::cout << (std::forward<Args>(args)), 0)...};
}

template <class... Args>
void logg(LogMode m, Args&&... x) {
	if (g_log_mode < m) return;
	if (m == I)
		std::cout << "Info: ";
	else if (m == W)
		std::cout << "Warning: ";
	else if (m == E)
		std::cout << "Error: ";
	logg(std::forward<Args>(x)...);
}

// NAL unit types.
enum {
	NAL_SLICE = 1,      // Non keyframe.
	NAL_DPA = 2,
	NAL_DPB = 3,
	NAL_DPC = 4,
	NAL_IDR_SLICE = 5,  // Keyframe.
	NAL_SEI = 6,
	NAL_SPS = 7,
	NAL_PPS = 8,
	NAL_AUD = 9,
	NAL_END_SEQUENCE = 10,
	NAL_END_STREAM = 11,
	NAL_FILLER_DATA = 12,
	NAL_SPS_EXT = 13,
	NAL_AUXILIARY_SLICE = 19,
	NAL_FF_IGNORE = 0xff0f001,
};


// Swap the 8-bit bytes into their reverse order.
uint16_t swap16(uint16_t us);
uint32_t swap32(uint32_t ui);
uint64_t swap64(uint64_t ull);


// Unaligned access of 8-bit bytes.
static_assert(sizeof(uint8_t) == 1 && sizeof(uint32_t) == 4,
			  "Unsupported machine byte size");

// Read an unaligned, big-endian value.
// A compiler will optimize this (at -O2) to a single instruction if possible.
template <typename T, size_t N = sizeof(T)>
constexpr T readBE(const uint8_t* p, size_t i = 0) {
	return (i >= N) ? T(0) :
		(T(*p) << ((N - 1 - i) * 8)) | readBE<T,N>(p + 1, i + 1);
}

template <typename T>
constexpr void readBE(T* result, const uint8_t* p) { *result = readBE<T>(p); }

// Write an unaligned, big-endian value.
template <typename T, size_t N = sizeof(T)>
constexpr void writeBE(uint8_t* p, T value, size_t i = 0) {
	(i >= N) ? void(0) :
		(*p = (value >> ((N - 1 - i) * 8)) & 0xFF
		 , writeBE<T,N>(p + 1, value, i + 1));
}

#if 0  // Unused
// Read an unaligned value in native-endian format.
// Encode the unaligned access intention by using memcpy() with its
//  destination and source pointing to types with the wanted alignment.
// Some compilers use the alignments of these types for further optimizations.
// A compiler can optimize this memcpy() into a single instruction.
template <typename T>
constexpr T readNE(const uint8_t* p) {
	T value;
	std::memcpy(&value, p, sizeof(value));
	return value;
}

template <typename T>
constexpr void readNE(T* result, const uint8_t* p) {
	std::memcpy(result, p, sizeof(result));
}
#endif


// Read an unaligned, Golomb encoded value. 
int readGolomb(const uchar** buffer, int* bit_offset);

// Read bits from an unaligned buffer. 
uint readBits(int numbits, const uchar** buffer, int* bit_offset);


// Convert an Atom name to its Id code.
constexpr uint32_t name2Id(const unsigned char* p, size_t i = 0) {
	return (i >= 4) ? 0U :
		(uint32_t(*p) << ((3 - i) * 8)) | name2Id(p + 1, i + 1);
}

constexpr uint32_t name2Id(const char* p, size_t i = 0) {
	return (i >= 4) ? 0U :
		(uint32_t(uint8_t(*p)) << ((3 - i) * 8)) | name2Id(p + 1, i + 1);
}

extern uint32_t name2Id(const std::string& name);

// Convert an Atom Id code to its name.
// Note: Some Atoms may contain non-printable chars (e.g. in user-data).
constexpr void id2Name(char* p, uint32_t id, size_t i = 0) {
	(i >= 4) ? void(0) :
		(*p = (id >> ((3 - i) * 8)) & 0xFF , id2Name(p + 1, id, i + 1));
}

extern void id2Name(std::string* name, uint32_t id);

void printBuffer(const uchar* pos, int n);
size_t hexDump(const uchar *p, size_t n, size_t addr);


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // COMMON_H_
