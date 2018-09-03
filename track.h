//==================================================================//
/*                                                                  *
	Untrunc - track.h

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
 *                                                                  */
//==================================================================//

#ifndef TRACK_H_
#define TRACK_H_

#include <cstdint>
#include <vector>

#include "codec.h"


struct AVCodecContext;
class Atom;


class Track {
public:
	Atom* trak_;
	uint32_t timescale_;
	uint32_t duration_;
	int n_matched;
	Codec codec_;

	std::vector<int> times_;
	std::vector<int> offsets_;
	std::vector<int> sizes_;
	std::vector<int> keyframes_;  // Used for 'avc1', 0 based!

	//Track(): trak_(0) { }
	Track(Atom* t, AVCodecContext* c);

	void parse(Atom* mdat);
	void writeToAtoms();
	void clear();
	void fixTimes();

	std::vector<int> getSampleTimes(Atom* t);
	std::vector<int> getKeyframes(Atom* t);
	std::vector<int> getSampleSizes(Atom* t);
	std::vector<int> getChunkOffsets(Atom* t);
	std::vector<int> getSampleToChunk(Atom* t, int nchunks);

	void saveSampleTimes();
	void saveKeyframes();
	void saveSampleToChunk();
	void saveSampleSizes();
	void saveChunkOffsets();
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // TRACK_H_
