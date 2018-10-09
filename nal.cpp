//==================================================================//
/*                                                                  *
	Untrunc - nal.cpp

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

#include "nal.h"

#include <cassert>
#include <cstring>  // For: memcpy
#include <iostream>


using std::cout;
using std::vector;


// AVC1

NalInfo::NalInfo(const uchar* start, int max_size) {
	is_ok = parseNal(start, max_size);
}

// Return false means this probably is not a NAL.
bool NalInfo::parseNal(const uchar* buffer, uint32_t maxlength) {
	//cout << "parsing nal here:\n";
	//printBuffer(buffer, 30);

	if (buffer[0] != 0) {
		logg(V, "First byte expected 0\n");
		return false;
	}
	// This is supposed to be the length of the NAL unit.
	// FIXIT: Only true if 'avcC' bytestream standard used, not 'Annex B'!
	uint32_t len = swap32(*reinterpret_cast<const uint32_t*>(buffer));
	length_ = len + 4;
	logg(V, "Length: ", length_, "\n");


	const uint32_t MAX_AVC1_LENGTH = 8u * 1024 * 1024;  // 8 MiB.
	if (len > MAX_AVC1_LENGTH) {
		logg(V, "Max length exceeded\n");
		return false;
	}

	if (length_ > maxlength) {
		logg(W, "buffer exceeded by: ", len - maxlength, '\n');
		return false;
	}
	buffer += 4;
	if (*buffer & (1 << 7)) {
		logg(V, "Warning: Forbidden first bit 1\n");
		is_forbidden_set_ = true;
		// Means payload is garbage, header might be ok though,
		// so don't return false.
	}
	ref_idc_ = *buffer >> 5;
	logg(V, "Ref idc: ", ref_idc_, "\n");

	nal_type_ = *buffer & 0x1f;
	logg(V, "Nal type: ", nal_type_, "\n");
	if (nal_type_ != NAL_SLICE && nal_type_ != NAL_IDR_SLICE &&
		nal_type_ != NAL_SPS)
		return true;

	// Check if size is reasonable.
	if (len < 8) {
		cout << "Too short!\n";
		return false;
	}

	buffer++;  // Skip NAL header.

#if 0
	// Remove the emulation prevention 3 byte.
	// Could be done in place to speed things up.
	// EDIT: Only needed for 'annex b' bytestream standard, which
	//       is currently not supported anyways. See nal-decoder.
	// FIXIT: Citation needed.
	payload_.reserve(len);
	for (unsigned int i = 0; i < len; i++) {
		if (i + 2 < len &&
			buffer[i] == 0 && buffer[i + 1] == 0 && buffer[i + 2] == 3) {
			payload_.push_back(buffer[i]);
			payload_.push_back(buffer[i + 1]);
			assert(buffer[i + 2] == 0x3);
			i += 2;  // Skipping 0x3 byte!
		} else {
			payload_.push_back(buffer[i]);
		}
	}
#else
	payload_.resize(len);
	memcpy(payload_.data(), buffer, len);
#endif
	return true;
}


// vim:set ts=4 sw=4 sts=4 noet:
