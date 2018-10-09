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
	Atom*    trak_      = nullptr;
	uint32_t timescale_ = 0;
	uint32_t duration_  = 0;
	uint32_t n_matched_ = 0;
	Codec    codec_;

	std::vector<uint32_t> times_;
	std::vector<uint32_t> keyframes_;  // Used for avc1, 0 based!
	std::vector<uint32_t> sizes_;
	std::vector<uint64_t> file_offsets_;

	Track(Atom* trak, AVCodecContext* context);

	bool parse(Atom* mdat);
	void clear();
	void writeToAtoms();
	void fixTimes();

private:
	void cleanUp();

	static std::vector<uint32_t> getSampleTimes(Atom* trak);
	static std::vector<uint32_t> getKeyframes(Atom* trak);
	static std::vector<uint32_t> getSampleSizes(Atom* trak);
	static std::vector<uint64_t> getChunkOffsets(Atom* trak);
	static std::vector<uint32_t> getSampleToChunk(Atom* trak,
												  uint32_t num_chunks);

	void saveSampleTimes();
	void saveKeyframes();
	void saveSampleSizes();
	void saveSampleToChunk();
	void saveChunkOffsets();
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // TRACK_H_
