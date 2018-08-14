//==================================================================//
/*                                                                  *
	Untrunc - avc-config.h

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

#ifndef AVC_CONFIG_H_
#define AVC_CONFIG_H_

#include "atom.h"
#include "common.h"


class SpsInfo;


class AvcConfig {
public:
	AvcConfig() = default;
	explicit AvcConfig(const Atom& stsd);
	~AvcConfig();

	bool is_ok = false;
	SpsInfo* sps_info_ = NULL;

private:
	bool decode(const uchar* start);
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // AVC_CONFIG_H_
