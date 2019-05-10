/*
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

														*/

#ifndef TRACK_H
#define TRACK_H

#include <vector>
#include <iosfwd>

#include "codec.h"
class Codec;

class Track {
public:
	Track(Atom* t, AVCodecParameters* c, int mp4_timescale);

	Atom *trak_;
	Codec codec_;
	int timescale_;
	int mp4_timescale_;
	int duration_; // normally sum of sample times
	int n_matched;
	double stretch_factor_ = 1; // stretch video by via stts entries
	bool do_stretch_ = false;
	std::string type_; // 'soun' OR 'vide'

	std::vector<int> times_; // sample times
	std::vector<uint> offsets_;
	std::vector<int64_t> offsets64_;
	std::vector<int> sizes_;
	std::vector<int> keyframes_; //used for 'avc1', 0 based!

	void parse(Atom *mdat);
	void writeToAtoms();
	void clear();
	void fixTimes(bool is64);

	int getDurationInTimescale(); // in movie timescale, not track timescale
	int getDurationInMs();

	std::vector<int> getSampleTimes(Atom *t);
	std::vector<int> getKeyframes(Atom *t);
	std::vector<int> getSampleSizes(Atom *t);
	std::vector<uint> getChunkOffsets(Atom* t);
	std::vector<int> getSampleToChunk(Atom *t, int nchunks);

	std::vector<int64_t> getChunkOffsets64(Atom* t);

	void saveSampleTimes();
	void saveKeyframes();
	void saveSampleToChunk();
	void saveSampleSizes();
	void saveChunkOffsets();
};



#endif // TRACK_H
