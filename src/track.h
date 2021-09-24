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

	Atom *trak_ = nullptr;
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
	void getCompositionOffsets();
	void genPatternPerm();

	void saveSampleTimes();
	void saveKeyframes();
	void saveSampleToChunk();
	void saveSampleSizes();
	void saveChunkOffsets();
	void saveCompositionOffsets();

	bool isChunkTrack();

	std::string getCodecNameSlow();

	// sample size stats
	uint avg_ss_normal_ = 0, min_ss_normal_ = 0, max_ss_normal_ = 0,
	    avg_ss_keyframe_ = 0, min_ss_keyframe_ = 0, max_ss_keyframe_ = 0,
	    avg_ss_total_ = 0, min_ss_total_ = 0, max_ss_total_ = 0;
	bool be_strict_maxpart_ = false;
	uint max_allowed_ss_ = 0;

	struct Chunk {
		Chunk() = default;
		Chunk(off_t off, int64_t size, int ns);
		off_t off_ = 0;  // absolute offset
		int64_t already_excluded_ = 0;
		int64_t size_ = 0;
		int n_samples_ = 0;
	};

	// we try to predict next track_idx with these
	std::vector<std::vector<MutualPattern>> dyn_patterns_;  // track_idx -> MutualPatterns

	std::vector<Chunk> chunks_;
	std::vector<int> likely_n_samples_;  // per chunk
	std::vector<int> likely_sample_sizes_;
	double likely_n_samples_p = 0;
	double likely_samples_sizes_p = 0;
	bool hasPredictableChunks();
	bool shouldUseChunkPrediction();
	int64_t chunk_distance_gcd_;
	int64_t start_off_gcd_;
	int64_t end_off_gcd_;  // e.g. after free sequence
	bool isChunkOffsetOk(off_t off);
	int64_t stepToNextOwnChunk(off_t off);
	int64_t stepToNextOwnChunkAbs(off_t off);
	int64_t stepToNextOtherChunk(off_t off);
	bool is_dummy_ = false;
	bool dummyIsUsedAsPadding();

	Track::Chunk current_chunk_;
	bool chunkMightBeAtAnd();

	void printDynStats();
	void printDynPatterns(bool show_percentage=false);
	void genLikely();
	bool isSupported() { return codec_.isSupported(); }
	bool hasZeroTransitions();

	int useDynPatterns(off_t offset);
	void genChunkSizes();

	void pushBackLastChunk();
	bool doesMatchTransition(const uchar* buff, int track_idx);

	void applyExcludedToOffs();

	std::vector<int> orig_comp_offs_;  // from ctts
	int dump_idx_ = 0;
	bool chunkReachedSampleLimit();
	int has_duplicates_ = false;
private:
	// from healthy file
	std::vector<int> orig_sizes_;
	std::vector<int> orig_times_;
	std::vector<std::pair<int, int>> orig_ctts_;

	std::vector<uint> dyn_patterns_perm_;

	int use_looks_like_twos_idx_ = -1;

	uint ownTrackIdx();
	void calcAvgSampleSize();
	void setMaxAllowedSampleSize();
};

std::ostream& operator<<(std::ostream& out, const Track::Chunk& fi);


#endif // TRACK_H
