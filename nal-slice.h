//==================================================================//
/*                                                                  *
	Untrunc - nal-slice.h

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

#ifndef NAL_SLICE_H_
#define NAL_SLICE_H_

#include <cstdint>


class NalInfo;
class SpsInfo;


class SliceInfo {
public:
	int first_mb        = 0;  // Unused.
	int slice_type      = 0;  // Should match the nal type (1, 5).
	int pps_id          = 0;  // Which parameter set to use.
	int frame_num       = 0;
	int field_pic_flag  = 0;
	int bottom_pic_flag = 0;  // Only if field_pic_flag.
	int idr_pic_id      = 0;  // Read only for nal_type 5.
	int poc_type        = 0;  // If zero, check the lsb.
	int poc_lsb         = 0;

	int idr_pic_flag    = 0;  // 1 = nal_type 5, 0 = nal_type 1.
	bool is_ok          = false;

	SliceInfo() = default;
	SliceInfo(const NalInfo&, const SpsInfo&);

	bool isInNewFrame(const SliceInfo&);

private:
	bool decode(const NalInfo&, const SpsInfo&);
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // NAL_SLICE_H_
