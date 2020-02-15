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

#include "track.h"
class Atom;
class BufferedAtom;
class FileRead;
class AVFormatContext;
class FrameInfo;

class Mp4 : public HasHeaderAtom {
friend Track;
public:
    Mp4() = default;
	~Mp4();

	void parseOk(const std::string& filename); // parse the healthy one

	void printTracks();
	void printAtoms();
	void printDynStats();
	void printMediaInfo();

	void makeStreamable(const std::string& ok, const std::string& output);
	void saveVideo(const std::string& filename);

	void dumpSamples();
	void analyze(bool gen_off_map=false);
	void repair(const std::string& filename);

	bool wouldMatch(off_t offset, const std::string& skip = "", bool force_strict=false, int last_track_idx=-1);
	bool wouldMatchDyn(off_t offset, int last_idx);
	FrameInfo getMatch(off_t offset, bool force_strict=false);
	void analyzeOffset(const std::string& filename, off_t offset);

	bool hasCodec(const std::string& codec_name);
	uint getTrackIdx(const std::string& codec_name);
	std::string getCodecName(uint track_idx);
	Track& getTrack(const std::string& codec_name);
	std::string offToStr(off_t offset);

	static uint64_t step_;  // step_size in unknown sequence
	std::vector<Track> tracks_;
//	static const int pat_size_ = 64;
	static const int pat_size_ = 32;
	int idx_free_ = -1;  // idx of dummy track

	class Chunk : public Track::Chunk {
	public:
		Chunk() = default;
		Chunk(off_t off, int ns, int track_idx, int sample_size);
		explicit operator bool() { return track_idx_ > 0; }
		int track_idx_ = -1;
		int sample_size_ = 0;
	};

private:
	Atom *root_atom_ = nullptr;
	BufferedAtom* findMdat(FileRead& file_read);
	AVFormatContext *context_;

	void parseTracksOk();
	void chkStrechFactor();
	void setDuration();
	void chkUntrunc(FrameInfo& fi, Codec& c, int i);
	void addFrame(const FrameInfo& frame_info);
	bool chkOffset(off_t& offset);  // updates offset
	const uchar* loadFragment(off_t offset);
	bool broken_is_64_ = false;
	int unknown_length_ = 0;
	uint64_t pkt_idx_ = 0;
	int last_track_idx_ = -1;  // 0th track is most relialbale
	std::vector<int> unknown_lengths_;

	std::string filename_ok_;
	bool use_offset_map_ = false;
	std::map<off_t, FrameInfo> off_to_frame_;
	std::map<off_t, Mp4::Chunk> off_to_chunk_;
	void chkFrameDetectionAt(FrameInfo& detected, off_t off);
	void chkChunkDetectionAt(Mp4::Chunk& detected, off_t off);
	void dumpMatch(const FrameInfo& fi, int idx);
	void dumpChunk(const Mp4::Chunk& chunk, int& idx);
	void dumpIdxAndOff(off_t off, int idx);
	std::vector<FrameInfo> to_dump_;

	void genDynStats();
	void genChunks();
	void genChunkTransitions();
	void genDynPatterns();
	off_t first_off_rel_ = -1;  // relative to mdat
	off_t first_off_abs_ = -1;
	std::map<std::pair<int, int>, std::vector<off_t>> chunk_transitions_;

	buffs_t offsToBuffs(const offs_t& offs, const std::string& load_prefix);
	patterns_t offsToPatterns(const offs_t& offs, const std::string& load_prefix);

	Mp4::Chunk fitChunk(off_t offset, uint track_idx);

	void noteUnknownSequence(off_t offset);
	void addUnknownSequence(off_t start, uint64_t length);
	void addToExclude(off_t start, uint64_t length, bool force=false);

	int64_t calcStep(off_t offset);

	const std::vector<std::string> ignore_duration_ = {"tmcd", "fdsc"};

	const uchar* current_fragment_;
	uint current_maxlength_;
	BufferedAtom* current_mdat_ = nullptr;

	FileRead& openFile(const std::string& filename);
	FileRead* current_file_ = nullptr;

	Mp4::Chunk getChunkPrediction(off_t offset);
	bool tryMatch(off_t& off);
	bool tryChunkPrediction(off_t& off);

	void printOffset(off_t offset);


	bool pointsToZeros(off_t offset);
	bool isAllZerosAt(off_t offset, int n);

	const uchar* getBuffAround(off_t offset, int64_t n);

	void removeEmptyTraks();
	bool needDynStats();
	bool shouldBeStrict(off_t off, int track_idx);
	void checkForBadTracks();

	std::string getOutputSuffix();
	bool chkNeedOldApi();
};

class FrameInfo {
public:
	FrameInfo() = default;
	FrameInfo(int track_idx, Codec& c, off_t offset, uint length);
	FrameInfo(int track_idx, bool was_keyframe, uint audio_duration, off_t offset, uint length);
	explicit operator bool();
	int track_idx_;

	bool keyframe_;
	uint audio_duration_;
	off_t offset_;
	uint length_ = 0;
	bool should_dump_;
};
bool operator==(const FrameInfo& lhs, const FrameInfo& rhs);
bool operator!=(const FrameInfo& lhs, const FrameInfo& rhs);
std::ostream& operator<<(std::ostream& out, const FrameInfo& fi);

bool operator==(const Mp4::Chunk& lhs, const Mp4::Chunk& rhs);
bool operator!=(const Mp4::Chunk& lhs, const Mp4::Chunk& rhs);
std::ostream& operator<<(std::ostream& out, const Mp4::Chunk& c);

#endif // MP4_H
