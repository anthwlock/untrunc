//==================================================================//
/*                                                                  *
	Untrunc - nal.h

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

#ifndef NAL_H_
#define NAL_H_

#include <cstdint>
#include <vector>

#include "common.h"


class NalInfo {
public:
	NalInfo() = default;
	NalInfo(const uchar* start, int max_size);

	uint32_t length_ = 0;
	int ref_idc_ = 0;
	int nal_type_ = 0;

	bool is_ok;  // did parsing work
	bool is_forbidden_set_;
	std::vector<uchar> payload_;
	bool parseNal(const uchar* start, uint32_t max_size);
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // NAL_H_
