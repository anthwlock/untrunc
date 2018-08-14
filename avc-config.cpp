//==================================================================//
/*                                                                  *
	Untrunc - avc-config.cpp

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

#include "avc-config.h"

#include "sps-info.h"


AvcConfig::AvcConfig(const Atom& stsd) {
	// find avcC payload
	const uchar* start = stsd.content_.data() + 12;
	char pattern[5] = "avcC";
	int found = 0;
	int limit = stsd.length_ - 16;
	while (limit--) {
		if (*start++ == pattern[found])
			found++;
		else if (found)
			found = 0;
		if (found == 4)
			break;
	}
	if (found != 4) {
		logg(V, "avcC signature not found\n");
		is_ok = false;
		return;
	}
	int off = start - stsd.content_.data();
	int len = stsd.length_ - off;
	logg(V, "found avcC after: ", off, '\n');
	logg(V, "remaining len:", len, '\n');

	is_ok = decode(start);
}

AvcConfig::~AvcConfig() { delete sps_info_; }

bool AvcConfig::decode(const uchar* start) {
	logg(V, "parsing avcC ...\n");
	int off = 0;
	int ver = readBits(8, start, off);  // config_version.
	if (ver != 1) {
		logg(V, "avcC config version != 1\n");
		return false;
	}
	start += 4;
	uint reserved = readBits(3, start, off);  // 111.
	if (reserved != 7) {
		logg(V, "avcC - reserved is not reserved: ", reserved, '\n');
		return false;
	}
	uint num_sps = readBits(5, start, off);
	if (num_sps != 1)
		logg(W, "avcC contains more than 1 SPS");
	uint len_sps = readBits(16, start, off);
	logg(V, "len_sps: ", len_sps, '\n');
	sps_info_ = new SpsInfo(start);
	return sps_info_->is_ok;
}


// vim:set ts=4 sw=4 sts=4 noet:
