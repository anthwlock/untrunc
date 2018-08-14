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
#include <map>
#include <string>
#include <utility>


typedef unsigned int uint;
typedef unsigned char uchar;

enum LogMode { E, W, I, V, VV };
extern LogMode g_log_mode;

extern size_t g_max_partsize;

template <class... Args>
void logg(Args&&... args) {
	//(std::cout << ... << args); // Binary left fold (C++17).
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
	{"stco", "chunk to offset"}};


uint16_t swap16(uint16_t us);
uint32_t swap32(uint32_t ui);
uint64_t swap64(uint64_t ull);


int readGolomb(const uchar** buffer, int* offset);
uint readBits(int n, const uchar** buffer, int* offset);

void printBuffer(const uchar* pos, int n);


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // COMMON_H_
