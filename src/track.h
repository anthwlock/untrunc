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

#include <limits>
#include <vector>
#include <iomanip>  // setw

#include "codec.h"
#include "mutual_pattern.h"

struct SSTats {
	uint min = std::numeric_limits<uint>::max(),
		max=0,
		upper_bound=0, lower_bound=0,
		lower_bound_2=0;  // even lower
	uint64_t n=0;
	double avg=0, m2=0, dev=0, rel_dev=0, z_score_max=0, z_score_min=0;

	void updateStat(uint sz) {
		n++;
		double delta = sz - avg;
		avg += delta/n;
		m2 += delta * (sz - avg);

		min = std::min(min, sz);
		max = std::max(max, sz);
	};

	void onFinished() {
		if (!n) {
			min = 0;
			return;
		}
		dev = sqrt((double)m2 / n);
		rel_dev = dev / avg;

		z_score_max = (max - avg) / dev;
		z_score_min = (avg - min) / dev;

		double z_score;
		if ((z_score_max > 2.5 && rel_dev > 0.025) || (z_score_max > 2 && n < 35)) {
			upper_bound = avg + 7*(max - min);
			dbgg("fallback to save upper_bound", upper_bound);
		} else {
			z_score = std::max(std::max(2.326, z_score_max+1), z_score_max*1.5);
			upper_bound = ceil(avg + dev * z_score);
		}

		if ((z_score_min > 2.5 && rel_dev > 0.025) || (z_score_min > 1 && n < 35)) {
			lower_bound = 0.8*min;
			dbgg("fallback to save lower_bound", lower_bound);
		} else {
			z_score = std::max(std::max(2.326, z_score_min+1), z_score_min*1.5);
			lower_bound = ceil(std::max(0., avg - dev * z_score));
		}

		lower_bound_2 = lower_bound >> 5;
	}

	void onConstant(uint sz) {
		min = avg = max = upper_bound = lower_bound = lower_bound_2 = sz;
		n = 100;
	}

};

inline
std::ostream& operator<<(std::ostream& out, const SSTats& ss) {
	return out << std::right
		<< std::setw(8) << ss.min << " "
		<< std::setw(8) << int(round(ss.avg))  << " "
		<< std::setw(8) << ss.max << " |"
		<< std::setw(8) << ss.lower_bound << " "
		<< std::setw(8) << ss.upper_bound << " |"
		<< std::setw(8) << std::fixed << std::setprecision(5) << ss.rel_dev << " "
		<< std::setw(8) << ss.z_score_min << " "
		<< std::setw(8) << ss.z_score_max << " "
		<< std::setw(8) << ss.n << " "
	;
}

struct SampleSizeStats {
	SSTats normal, keyframe, effective_keyframe;

	void updateStat(int sz, bool is_keyframe) {
		if (is_keyframe) keyframe.updateStat(sz);
		else normal.updateStat(sz);
	}

	void onFinished() {
		normal.onFinished();
		keyframe.onFinished();

		if (!keyframe.n || normal.upper_bound > keyframe.upper_bound) {
			effective_keyframe = normal;
		} else {
			effective_keyframe = keyframe;
		}
	}

	void onConstant(int sz) {
		normal.onConstant(sz);
		keyframe.onConstant(sz);
		effective_keyframe.onConstant(sz);
	}

	// for estimating chunk size
	int averageSize() {
		auto n = normal.n + keyframe.n;
		return normal.avg * ((double)normal.n / n) + keyframe.avg * ((double)keyframe.n / n);
	}

	uint maxAllowedPktSz() {
		return std::max(normal.upper_bound, keyframe.upper_bound);
	}

	SSTats& effectiveStat(bool is_keyframe) {
		return is_keyframe ? effective_keyframe : normal;
	}

	uint getUpperLimit(bool is_keyframe) {
		return effectiveStat(is_keyframe).upper_bound;
	}

	uint getLowerLimit(bool is_keyframe) {
		return effectiveStat(is_keyframe).lower_bound;
	}

	bool exceedsAllowed(uint sz, bool is_keyframe) {
		uint limit = getUpperLimit(is_keyframe);
		// dbgg("exceedsAllowed? ", limit, sz, is_keyframe, keyframe.upper_bound, effective_keyframe.upper_bound);
		return limit && sz > limit;
	}

	bool isBigEnough(uint sz, bool is_keyframe) {
		uint limit = getLowerLimit(is_keyframe);
		return limit && sz >= limit;
	}

	bool likelyTooSmall(uint sz) {
		auto& s = normal;
		if (sz < s.lower_bound_2 && s.n > 20 && s.rel_dev < 1) {
			dbgg("likelyTooSmall", sz, s.lower_bound_2);
			return true;
		}
		return false;
	}

	bool wouldExceed(const char *label, uint length, uint additional, bool is_keyframe) {
		if (exceedsAllowed(length + additional, is_keyframe)) {
			logg(V, "new partial ", label, "-length would exceed upper_bound: ", length, " + ", additional, " = ", length + additional, " > ", getUpperLimit(is_keyframe), "  // is_keyframe=", is_keyframe, "\n");
			return true;
		}
		return false;
	}
};

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
	bool is_tmcd_hardcoded_ = false;

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
	void mergeChunks();
	void splitChunks();

	bool isChunkTrack();

	std::string getCodecNameSlow();

	SampleSizeStats ss_stats_;

	int pkt_sz_gcd_ = 1;
	int alignPktLength(int length) {
		length += (pkt_sz_gcd_ - (length % pkt_sz_gcd_)) % pkt_sz_gcd_;
		return length;
	}
	int getSizeWithGcd(size_t idx) {
		return alignPktLength(getSize(idx));
	}

	int pad_after_chunk_ = -1;
	int last_pad_after_chunk_ = -1;  // helper so we can skip last chunk (padding might be different there)
	std::vector<uint8_t> has_unclear_transition_;  // track_idx -> has_unclear_transition

	void adjustPadAfterChunk(int new_pad) {
		if (last_pad_after_chunk_ != -1) {
			auto x = last_pad_after_chunk_;
			if (pad_after_chunk_ == -1) {
				pad_after_chunk_ = x;
			} else if (pad_after_chunk_ != x) {
				pad_after_chunk_ = 0;
			}
		}
		last_pad_after_chunk_ = new_pad;
	}

	struct Chunk {
		Chunk() = default;
		Chunk(off_t off, int64_t size, int ns);
		off_t off_ = 0;  // absolute offset
		int64_t already_excluded_ = 0;
		int64_t size_ = 0;  // only updated for 'free'
		int n_samples_ = 0;
	};

	// we try to predict next track_idx with these
	std::vector<std::vector<MutualPattern>> dyn_patterns_;  // track_idx -> MutualPatterns
	int predictable_start_cnt_ = 0;
	int unpredictable_start_cnt_ = 0;

	std::vector<Chunk> chunks_;
	std::vector<int> likely_n_samples_;  // per chunk
	std::vector<int> likely_sample_sizes_;
	double likely_n_samples_p = 0;
	double likely_samples_sizes_p = 0;
	bool hasPredictableChunks();
	bool shouldUseChunkPrediction();
	int64_t chunk_distance_gcd_;  // to next chunk of same track

	// these offsets are absolute (to file begin)
	int64_t start_off_gcd_;
	int64_t end_off_gcd_;  // sometimes 'free' sequences are used for padding to absolute n*32kb offsets

	bool isChunkOffsetOk(off_t off);
	int64_t stepToNextOwnChunk(off_t off);
	int64_t stepToNextOwnChunkAbs(off_t off);
	int64_t stepToNextOtherChunk(off_t off);
	bool is_dummy_ = false;
	bool dummyIsUsedAsPadding();

	Track::Chunk current_chunk_;
	bool chunkProbablyAtAnd();

	void printStats();
	void printDynPatterns(bool show_percentage=false);
	void genLikely();
	bool isSupported() {
		return codec_.isSupported() || is_tmcd_hardcoded_;
	}
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
};

std::ostream& operator<<(std::ostream& out, const Track::Chunk& fi);


#endif // TRACK_H
