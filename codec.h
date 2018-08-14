//==================================================================//
/*                                                                  *
	Untrunc - codec.h

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

#ifndef CODEC_H_
#define CODEC_H_

#include <string>
#include <vector>

#include "common.h"


struct AVCodecContext;
struct AVCodec;
class Atom;
class AvcConfig;
class AudioConfig;


class Codec {
public:
	explicit Codec(AVCodecContext* c);

	std::string name_;

	void parse(Atom* trak, std::vector<int>& offsets, Atom* mdat);
	bool matchSample(const uchar* start);
	int getLength(const uchar* start, uint maxlength, int& duration);

	// Used by "mp4a".
	int mask1_;
	int mask0_;
	AVCodecContext* context_;
	AVCodec* codec_;

	AvcConfig* avc_config_;
	AudioConfig* audio_config_;

	bool last_frame_was_idr_;
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // CODEC_H_
