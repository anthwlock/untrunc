/*
	Untrunc - mp4.h

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

#ifndef MP4_H
#define MP4_H

#include <vector>
#include <string>
#include <stdio.h>
#include <memory>

#include "common.h"
#include "track.h"
#include "atom.h"
class FileRead;
class AVFormatContext;
class FrameInfo;
class ChunkIt;
struct TrackGcdInfo;

struct WouldMatchCfg {
	off_t offset;
	const std::string& skip = "";
	bool force_strict = false;
	int last_track_idx = -1;
	bool very_first = false;
};
typedef WouldMatchCfg WMCfg;

inline
std::ostream& operator<<(std::ostream& out, const WouldMatchCfg& cfg) {
	return out << cfg.offset << ", "
		"skip=\"" << cfg.skip << "\", " <<
		"force_strict=" << cfg.force_strict << ", " <<
		"very_first=" << cfg.very_first;
}

struct FreeSeq {
	off_t offset;  // relative
	int64_t sz;
	int prev_track_idx;
	int64_t last_chunk_sz;

    bool operator<(const FreeSeq& other) const {
        return offset < other.offset;
    }
};

std::ostream& operator<<(std::ostream& out, const FreeSeq& x);

class Mp4 : public HasHeaderAtom {
friend Track;
friend Codec;
friend ChunkIt;
public:
    Mp4() = default;
	~Mp4();

	void parseOk(const std::string& filename, bool accept_unhealthy=false); // parse the first file

	void printTracks();
	void printTrackStats();
	void printAtoms();
	void printStats();
	void printMediaInfo();

	void makeStreamable(const std::string& ok, const std::string& output);
	void saveVideo(const std::string& filename);
	static void listm(const std::string& filename);
	static void unite(const std::string& mdat_fn, const std::string& moov_fn);
	static void shorten(const std::string& filename, int mega_bytes, bool force);

	void dumpSamples();
	void analyze(bool gen_off_map=false);
	void repair(const std::string& filename);

	// RSV file recovery
	bool detectRsvStructure(FileRead& file_read);
	void repairRsv(const std::string& filename);

	bool wouldMatch(const WouldMatchCfg& cfg);
	bool wouldMatch2(const uchar* start);
	bool wouldMatchDyn(off_t offset, int last_idx);
	FrameInfo predictSize(const uchar *start, int track_idx, off_t offset);
	FrameInfo getMatch(off_t offset, bool force_strict=false);
	void analyzeOffset(const std::string& filename, off_t offset);

	bool hasCodec(const std::string& codec_name);
	uint getTrackIdx(const std::string& codec_name);
	int getTrackIdx2(const std::string& codec_name) const;
	std::string getCodecName(uint track_idx);
	Track& getTrack(const std::string& codec_name);
	off_t toAbsOff(off_t offset);
	std::string offToStr(off_t offset);
	std::string getPathRepaired(const std::string& ok, const std::string& corrupt);
	bool alreadyRepaired(const std::string& ok, const std::string& corrupt);

	static uint64_t step_;  // step_size in unknown sequence
	std::vector<Track> tracks_;
//	static const int pat_size_ = 64;
	static const int pat_size_ = 32;
	int idx_free_ = kDefaultFreeIdx;  // idx of dummy track

	std::vector<FreeSeq> free_seqs_;  // for testing if 'free' is skippable
	std::vector<FreeSeq> chooseFreeSeqs();
	bool canSkipFree();

	bool has_moov_ = false;

	std::string ftyp_;
	off_t orig_mdat_start_;
	off_t orig_mdat_end_;

	bool premature_end_ = false;
	double premature_percentage_ = 0;

	// Chunks with constant sample size
	class Chunk : public Track::Chunk {
	public:
		Chunk() = default;
		Chunk(off_t off, int ns, int track_idx, int sample_size);
		explicit operator bool() { return track_idx_ >= 0; }
		int track_idx_ = -1;
		int sample_size_ = 0;
	};

	bool setDuplicateInfo();
	bool isExpectedTrackIdx(int i);
	void onFirstChunkFound(int track_idx);
	void correctChunkIdxSimple(int track_idx);

	// Note: An backtrack algo across multiple (e.g. track_order.size()) matches would be better than this, since it would work even if currentChunkFinished + we wouldn't have to check upfront
	int findSizeWithContinuation(off_t off, std::vector<int> sizes) {
		if (!track_order_.size() || amInFreeSequence() || currentChunkFinished(1)) {
			return sizes.back();
		}
		auto& t = tracks_[last_track_idx_];
		for (int i = sizes.size() - 1; i >= 0; --i) {
			auto sz = t.alignPktLength(sizes[i]);
			const uchar *buf = current_mdat_->getFragmentIf(off + sz, 1024);
			if (!buf) continue;
			bool matches = t.codec_.matchSample(buf);
			dbgg("findSizeWithContinuation", i, sizes[i], matches);
			if (matches) {
				return sizes[i];
			}
		}
		return sizes.back();
	}

private:
	Atom *root_atom_ = nullptr;
	static BufferedAtom* mdatFromRange(FileRead& file_read, BufferedAtom& mdat);
	static bool findAtom(FileRead& file_read, std::string atom_name, Atom& atom);
	BufferedAtom* findMdat(FileRead& file_read);
	AVFormatContext *context_;

	void parseHealthy();
	void parseTracksOk();
	void chkStrechFactor();
	void setDuration();
	void chkUntrunc(FrameInfo& fi, Codec& c, int i);
	void addFrame(const FrameInfo& frame_info);
	void addChunk(const Mp4::Chunk& chunk);

	int skipZeros(off_t& offset, const uchar* start);
	int skipAtomHeaders(off_t offset, const uchar *start);
	int skipAtoms(off_t offset, const uchar *start);
	bool advanceOffset(off_t& offset, bool just_simulate=false);
	bool chkOffset(off_t& offset);  // updates offset

	const uchar* loadFragment(off_t offset, bool update_cur_maxlen=true);
	bool broken_is_64_ = false;
	int64_t unknown_length_ = 0;
	std::vector<uint> atoms_skipped_;

	uint64_t pkt_idx_ = 0;

	// 0th track is most reliable
	// is equal to idx_free_ in case of unknown bytes, not padding
	int last_track_idx_ = -1;
	bool done_padding_ = false;
	bool done_padding_after_ = false;

	void setLastTrackIdx(int track_idx) {
		last_track_idx_ = track_idx;
		done_padding_ = false;
	}

	std::vector<int64_t> unknown_lengths_;

	std::string filename_ok_;
	bool use_offset_map_ = false;
	std::map<off_t, FrameInfo> off_to_frame_;
	std::map<off_t, Mp4::Chunk> off_to_chunk_;
	void chkDetectionAtImpl(FrameInfo* detectedFramePtr, Mp4::Chunk* detectedChunkPtr, off_t off);
	void chkFrameDetectionAt(FrameInfo& detected, off_t off);
	void chkChunkDetectionAt(Mp4::Chunk& detected, off_t off);
	void chkExpectedOff(off_t* expected_off, off_t real_off, uint sz, int idx);
	void dumpMatch(const FrameInfo& fi, int idx, off_t* predicted_off=nullptr);
	void dumpChunk(const Mp4::Chunk& chunk, int& idx, off_t* predicted_off=nullptr);
	void dumpIdxAndOff(off_t off, int idx);
	std::vector<FrameInfo> to_dump_;

	void genDynStats(bool force_patterns=false);
	void genChunks();
	void resetChunkTransitions();
	void genChunkTransitions();
	void collectPktGcdInfo(std::map<int, TrackGcdInfo> &track_to_info);
	void analyzeFree();
	void genDynPatterns();
	void genLikelyAll();
	off_t first_off_rel_ = -1;  // relative to mdat
	off_t first_off_abs_ = -1;
	std::map<std::pair<int, int>, std::vector<off_t>> chunk_transitions_;

	buffs_t offsToBuffs(const offs_t& offs, const std::string& load_prefix);
	patterns_t offsToPatterns(const offs_t& offs, const std::string& load_prefix);

	bool calcTransitionIsUnclear(int track_idx_a, int track_idx_b);
	void setHasUnclearTransition() {
		for (int i=0; i < tracks_.size(); i++) {
			auto &t = tracks_[i];
			t.has_unclear_transition_.clear();
			for (int j=0; j < tracks_.size(); j++) {
				auto v = calcTransitionIsUnclear(i, j);
				t.has_unclear_transition_.push_back(v);
			}
		}
	}


	bool anyPatternMatchesHalf(off_t offset, uint track_idx_to_try);
	Mp4::Chunk fitChunk(off_t offset, uint track_idx, uint known_n_samples=0);

	void addUnknownSequence(off_t start, uint64_t length);
	void addToExclude(off_t start, uint64_t length, bool force=false);

	bool chkUnknownSequenceEnded(off_t offset) {
		if (!unknown_length_) return false;
		disableNoiseBuffer();

		addToExclude(offset-unknown_length_, unknown_length_);
		unknown_lengths_.emplace_back(unknown_length_);
		unknown_length_ = 0;

		return true;
	}

	// Use if previous subroutine may have called addToExclude already
	void chkExcludeOverlap(off_t& start, int64_t& length) {
		auto last_end = (long long)current_mdat_->excludedEndOff();
		auto already_skipped = std::max(0LL, last_end - start);
		if (already_skipped) {
			start = last_end;
			length -= already_skipped;
		}
		assert(length >= 0, length);
	}

	int64_t calcStep(off_t offset);

	const std::vector<std::string> ignore_duration_ = {"tmcd", "fdsc"};

	uint current_maxlength_;
	BufferedAtom* current_mdat_ = nullptr;

	FileRead& openFile(const std::string& filename);
	FileRead* current_file_ = nullptr;

	bool predictChunkViaOrder(off_t offset, Mp4::Chunk& c);
	bool chunkStartLooksInvalid(off_t offset, const Mp4::Chunk& c);
	Chunk getChunkPrediction(off_t offset, bool only_perfect_fit=false);

	bool nearEnd(off_t offset) {
		return offset > (current_mdat_->contentSize() - cycle_size_);
	}

	bool amInFreeSequence() {
		return last_track_idx_ == idx_free_;
	}

	bool currentChunkFinished(int add_extra=0) {
		if (next_chunk_idx_ == 0) return true;
		auto [cur_track_idx, expected_ns] = track_order_[(next_chunk_idx_-1) % track_order_.size()];
		assert(cur_track_idx == last_track_idx_, cur_track_idx, getCodecName(cur_track_idx), last_track_idx_, getCodecName(last_track_idx_));
		auto& t = tracks_[cur_track_idx];
		if (t.current_chunk_.n_samples_ + add_extra < expected_ns) {
			dbgg("current_chunk not finished", next_chunk_idx_, t.codec_.name_, t.current_chunk_.n_samples_, expected_ns);
			return false;
		}
		return true;
	}

	void afterTrackRealloc() {
		for (int i=0; i < tracks_.size(); i++) {
			tracks_[i].codec_.onTrackRealloc(i);
		}
	}

	int getNextTrackViaDynPatterns(off_t offset) {
		int last_idx = done_padding_ && idx_free_ >= 0 ? idx_free_ : last_track_idx_;
		auto track_idx = tracks_[last_idx].useDynPatterns(offset);
		logg(V, "'", getCodecName(last_idx), "'.useDynPatterns() -> track_idx=", track_idx, "\n");
		return track_idx;
	}

	void addMatch(off_t& offset, FrameInfo& match);
	bool tryMatch(off_t& off);
	bool tryChunkPrediction(off_t& off);

	bool tryAll(off_t& offset) {
		if (shouldPreferChunkPrediction()) {
			logg(V, "trying chunkPredict first.. \n");
			if (tryChunkPrediction(offset) || tryMatch(offset)) return true;
		}
		else {
			if (tryMatch(offset) || tryChunkPrediction(offset)) return true;
		}
		return false;
	}

	void printOffset(off_t offset);


	bool pointsToZeros(off_t offset);
	bool isAllZerosAt(off_t offset, int n);

	const uchar* getBuffAround(off_t offset, int64_t n);

	void removeEmptyTraks();
	bool needDynStats();
	bool shouldBeStrict(off_t off, int track_idx);
	void checkForBadTracks();

	std::string getOutputSuffix();
	bool chkBadFFmpegVersion();

	bool shouldPreferChunkPrediction();
	bool currentChunkIsDone();
	int getChunkPadding(off_t& offset);

	Track* orig_first_track_;

	// repeating order (track_idx, n_samples) in mdat
	// ATM only recognizes very easy patterns
	std::vector<std::pair<int, int>> track_order_;
	std::vector<int> track_order_simple_;
	bool trust_simple_track_order_ = false;
	int cycle_size_ = 0;  // (average) size of single track_order_ repetition
	uint64_t next_chunk_idx_ = 0;  // does not count the 'free' track
	bool ignored_chunk_order_ = false;
	int getLikelyNextTrackIdx(int* n_samples=nullptr);
	bool isTrackOrderEnough();
	void genTrackOrder();
	void setDummyIsSkippable();
	void correctChunkIdx(int track_idx);
	bool dummy_is_skippable_ = false;
	int dummy_do_padding_skip_ = false;
	bool has_zero_transitions_ = false;

	int skipNextZeroCave(off_t off, int max_sz, int n_zeros);
	void pushBackLastChunk();

	void onNewChunkStarted(int new_track_idx) {
		if (ignored_chunk_order_) {
			dbgg("Ignored chunk order previously, calling correctChunkIdx", new_track_idx, getCodecName(new_track_idx));
			correctChunkIdx(new_track_idx);
			ignored_chunk_order_ = false;
		}

		pushBackLastChunk();
		done_padding_after_ = false;
		if (new_track_idx != idx_free_) {
			if (!first_chunk_found_) onFirstChunkFound(new_track_idx);
			next_chunk_idx_++;
		}
	}

	int twos_track_idx_ = -1;
	bool using_dyn_patterns_ = false;

	uint max_part_size_ = 0;
	bool first_chunk_found_ = false;

	int fallback_track_idx_ = -1;
	int calcFallbackTrackIdx();

	static const int kDefaultFreeIdx = -2;
};

class ChunkIt {

public:
	class Chunk : public Track::Chunk {
	public:
		Chunk() = default;
		Chunk(off_t off, int sz, int ns, int track_idx) : Track::Chunk(off, sz, ns), track_idx_(track_idx) {};
		Chunk(Track::Chunk t_chunk, int track_idx) : Track::Chunk(t_chunk), track_idx_(track_idx) {} ;
		explicit operator bool() { return track_idx_ >= 0; }
		int track_idx_ = -1;
		bool should_ignore_ = false;
	};

	const Mp4* mp4_;
	ChunkIt::Chunk current_;

	ChunkIt(const Mp4* mp4, bool do_filter, bool exclude_dummy) : mp4_(mp4), do_filter_(do_filter) {
		int n_real_tracks = mp4_->tracks_.size();
		if (exclude_dummy)
			if (mp4_->tracks_.back().is_dummy_) n_real_tracks--;
		cur_next_chunk_idx_.resize(n_real_tracks);
		mdat_end_ = mp4_->current_mdat_->start_ + mp4_->current_mdat_->length_;

		bad_tmcd_idx_ = mp4_->getTrackIdx2("tmcd");  // tmcd disturbs the 'order' array
		if (bad_tmcd_idx_ >= 0 && mp4->tracks_[bad_tmcd_idx_].chunks_[0].size_ > 4)  // seems legit, reset bad_tmcd_idx
			bad_tmcd_idx_ = -1;
		operator++();
	}
	static ChunkIt mkEndIt() { return ChunkIt(true); }

	void operator++() {
		next_chunk_idx_++;
		int track_idx = -1;
		auto off = std::numeric_limits<off_t>::max();
		for (uint i=0; i < cur_next_chunk_idx_.size(); i++) {
			if (cur_next_chunk_idx_[i] >= mp4_->tracks_[i].chunks_.size()) continue;
			auto toff = mp4_->tracks_[i].chunks_[cur_next_chunk_idx_[i]].off_;
			if (toff < off) {
				track_idx = i;
				off = toff;
			}
		}
		if (track_idx < 0) {becomeEndIt(); return;}
		if (off >= mdat_end_) {
			assert(g_ignore_out_of_bound_chunks);
			logg(W, "reached premature end of mdat\n");
			becomeEndIt();
			return;
		}
		current_ = ChunkIt::Chunk(mp4_->tracks_[track_idx].chunks_[cur_next_chunk_idx_[track_idx]], track_idx);
		cur_next_chunk_idx_[track_idx]++;

		if (next_chunk_idx_ < 10 && track_idx == bad_tmcd_idx_) {
			current_.should_ignore_ = true;
			if (do_filter_) operator++();
		}
	}

	ChunkIt::Chunk& operator*() {return current_;}
	bool operator!=(ChunkIt rhs) {
		return current_.track_idx_ != rhs.current_.track_idx_ ||
		     current_.off_ != rhs.current_.off_;
	}

private:
	off_t mdat_end_;
	int bad_tmcd_idx_ = -1;
	size_t next_chunk_idx_ = -1;
	std::vector<uint> cur_next_chunk_idx_;
	bool do_filter_;

	ChunkIt(bool is_end_it_) { assert(is_end_it_); becomeEndIt(); }
	void becomeEndIt() { current_ = ChunkIt::Chunk(-1, -1, -1, -1); }
};

/* Iteratable, which lists all chunks in order */
class AllChunksIn {
public:
	ChunkIt it_, end_it_;
	AllChunksIn(Mp4* mp4, bool do_filter, bool exclude_dummy=true) : it_(mp4, do_filter, exclude_dummy), end_it_(ChunkIt::mkEndIt()) {}
	ChunkIt begin() {return it_;}
	ChunkIt end() {return end_it_;}
};


class FrameInfo {
public:
	FrameInfo() = default;
	FrameInfo(int track_idx, Codec& c, off_t offset, uint length);
	FrameInfo(int track_idx, bool was_keyframe, uint audio_duration, off_t offset, uint length);
	explicit operator bool() const;
	int track_idx_;

	bool keyframe_;
	uint audio_duration_;
	off_t offset_;
	uint length_ = 0;
	bool should_dump_;
	uint pad_afterwards_ = 0;
};
bool operator==(const FrameInfo& lhs, const FrameInfo& rhs);
bool operator!=(const FrameInfo& lhs, const FrameInfo& rhs);
std::ostream& operator<<(std::ostream& out, const FrameInfo& fi);

bool operator==(const Mp4::Chunk& lhs, const Mp4::Chunk& rhs);
bool operator!=(const Mp4::Chunk& lhs, const Mp4::Chunk& rhs);
std::ostream& operator<<(std::ostream& out, const Mp4::Chunk& c);

#endif // MP4_H
