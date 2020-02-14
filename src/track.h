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

#include "codec.h"
#include "mutual_pattern.h"

class Track : public HasHeaderAtom {
public:
	Track(Atom* trak, AVCodecParameters* c, int mp4_timescale);
	Track(const std::string& codec_name);

	Atom *trak_;
	Codec codec_;
	int mp4_timescale_;
	double stretch_factor_ = 1; // stretch video by via stts entries
	bool do_stretch_ = false;
	std::string handler_type_; // 'soun' OR 'vide'
	std::string handler_name_;  // encoder used when created

	//mp4a can be decoded and repors the number of samples (duration in samplerate scale).
	//in some videos the duration (stts) can be variable and we can rebuild them using these values.
	std::vector<int> times_; // sample durations
	int constant_duration_ = -1;

	std::vector<int> sizes_;
	int constant_size_ = 0;
	std::vector<int> keyframes_; //used for 'avc1', 0 based!
	size_t num_samples_ = 0;
	int64_t getNumSamples() const;
	int getSize(size_t idx);
	int getTime(size_t idx);

	int getOrigSize(uint idx);

	void parseOk();
	void writeToAtoms(bool broken_is_64);
	void clear();
	void fixTimes();

	int64_t getDurationInTimescale(); // in movie timescale, not track timescale

	void getSampleTimes();
	void getKeyframes();
	void getSampleSizes();
	void getChunkOffsets();
	void parseSampleToChunk();
	void orderPatterns();

	std::vector<off_t> getChunkOffsets64();

	void saveSampleTimes();
	void saveKeyframes();
	void saveSampleToChunk();
	void saveSampleSizes();
	void saveChunkOffsets();

	struct Chunk {
		Chunk() = default;
		Chunk(off_t off, int64_t size, int ns);
		off_t off_ = 0;  // absolute offset
		int64_t alredy_excluded_ = 0;
		int64_t size_ = 0;
		int n_samples_ = 0;
	};

	std::vector<std::vector<MutualPattern>> dyn_patterns_;  // track_idx -> MutualPatterns
	std::vector<Chunk> chunks_;
	std::vector<int> likely_n_samples_;  // per chunk
	std::vector<int> likely_sample_sizes_;
	double likely_n_samples_p = 0;
	double likely_samples_sizes_p = 0;
	bool hasPredictableChunks();
	bool shouldUseChunkPrediction();
	int64_t chunk_distance_gcd_;
	bool isChunkOffsetOk(off_t off);
	int64_t stepToNextChunkOff(off_t off);
	bool is_dummy_ = false;

	Track::Chunk current_chunk_;
	bool chunkMightBeAtAnd();

	void printDynPatterns(bool show_percentage=false);
	void genLikely();
	bool isSupported() { return codec_.isSupported(); }

	int useDynPatterns(off_t offset);
	void genChunkSizes();

	void pushBackLastChunk();
	bool doesMatchTransition(const uchar* buff, int track_idx);

	void applyExcludedToOffs();

private:
	// from healthy file
	std::vector<int> orig_sizes_;
	std::vector<int> orig_times_;

	std::vector<uint> dyn_patterns_perm_;

};

std::ostream& operator<<(std::ostream& out, const Track::Chunk& fi);


#endif // TRACK_H
