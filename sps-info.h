//==================================================================//
/*                                                                  *
	Untrunc - sps-info.h

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

#ifndef SPS_INFO_H_
#define SPS_INFO_H_

#include "common.h"


class SpsInfo {
public:
	SpsInfo() = default;
	explicit SpsInfo(const uchar* pos);

	// Default values in case SPS is not decoded yet...
	int log2_max_frame_num = 4;
	bool frame_mbs_only_flag = true;
	int poc_type = 0;
	int log2_max_poc_lsb = 5;

	bool is_ok;
	bool decode(const uchar* pos);
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // SPS_INFO_H_
