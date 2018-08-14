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


using std::cerr;
using std::cout;
using std::dec;
using std::hex;


LogMode g_log_mode = LogMode::I;
size_t g_max_partsize = 1600000;


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


int readGolomb(const uchar** buffer, int* offset) {
	if (!buffer || !*buffer || !offset)
		return -1;
	const uchar* buf = *buffer;
	int          ofs = *offset;

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
			cerr << "Failed reading golomb: too large!\n";
			*buffer = buf;
			*offset = ofs;
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
	*offset = ofs;
	return res - 1;
}

uint readBits(int n, const uchar** buffer, int* offset) {
	if (!buffer || !*buffer || !offset)
		return -1;
	const uchar* buf = *buffer;
	int          ofs = *offset;

	uint res = 0;
	int d = 8 - ofs;
	uint mask = ((1 << d) - 1);
	int to_rshift = d - n;
	if (to_rshift > 0) {
		res = (*buf & mask) >> to_rshift;
		ofs += n;
	} else if (to_rshift == 0) {
		res = (*buf & mask);
		buf++;
		ofs = 0;
	} else {
		res = (*buf & mask);
		n -= d;
		buf++;
		ofs = 0;
		while (n >= 8) {
			res <<= 8;
			res |= *buf;
			n -= 8;
			buf++;
		}
		if (n > 0) {
			ofs = n;
			res <<= n;
			res |= *buf >> (8 - n);
		}
	}
	*buffer = buf;
	*offset = ofs;
	return res;
}

#if 0  // Not working correctly.
uint readBits(int n, uchar*& buffer, int& offset) {
	int res = 0;
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


void printBuffer(const uchar* pos, int n) {
	cout << hex;
	for (int i = 0; i != n; ++i) {
		int x = static_cast<int>(*(pos + i));
		if (x < 16) cout << '0';
		cout << x << ' ';
	}
	cout << dec << '\n';
}


// vim:set ts=4 sw=4 sts=4 noet:
