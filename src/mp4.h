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

class Mp4 {
public:
	int timescale_;
	int duration_ = 0;
	Atom *root_atom_;

	Mp4();
	~Mp4();

	void parseOk(std::string& filename); // parse the healthy one

	void printMediaInfo();
	void printAtoms();
	void makeStreamable(std::string& filename, std::string& output);

	void dumpSamples();
	void analyze(bool gen_off_map=false);
	void repair(std::string& filename, const std::string& filname_fixed);

	bool wouldMatch(off_t offset, const std::string& skip = "", bool strict=false);
	FrameInfo getMatch(off_t offset, bool strict);
	void analyzeOffset(const std::string& filename, off_t offset);

	bool hasCodec(const std::string& codec_name);
	uint getTrackIdx(const std::string& codec_name);
	std::string getCodecName(uint track_idx);
	Track& getTrack(const std::string& codec_name);

	static uint step_;  // step_size in unknown sequence

private:
	BufferedAtom* findMdat(FileRead& file_read);
	std::vector<Track> tracks_;
	AVFormatContext *context_;

	void saveVideo(const std::string& filename);
	void parseTracksOk();
	void chkStrechFactor();
	void setDuration();
	void chkUntrunc(FrameInfo& fi, Codec& c, int i);
	void addFrame(FrameInfo& frame_info);
	bool chkOffset(off_t& offset);  // updates offset
	const uchar* loadFragment(off_t offset, bool be_quiet=false);
	bool broken_is_64_ = false;
	int unknown_length_ = 0;
	uint64_t pkt_idx_ = 0;
	std::vector<int> unknown_lengths_;
	std::vector<int> audiotimes_stts_rebuild_;

	std::string filename_ok_;
	bool use_offset_map_ = false;
	std::map<off_t, FrameInfo> off_to_info_;
	void chkDetectionAt(FrameInfo& detected, off_t off);
	void dumpMatch(off_t off, const FrameInfo& fi, int idx);

	void noteUnknownSequence(off_t offset);
	void addToExclude(off_t start, uint64_t length, bool force=false);

	const std::vector<std::string> ignore_duration_ = {"tmcd", "fdsc"};

	const uchar* current_fragment_;
	uint current_maxlength_;
	BufferedAtom* current_mdat_ = nullptr;

};

class FrameInfo {
public:
	FrameInfo() = default;
	FrameInfo(uint track_idx, Codec& c, uint offset, uint length);
	FrameInfo(uint track_idx, bool was_keyframe, uint audio_duration, uint offset, uint length);
	operator bool();
	uint track_idx_;

	bool keyframe_;
	uint audio_duration_;
	off_t offset_;
	uint length_ = 0;
};
bool operator==(const FrameInfo& lhs, const FrameInfo& rhs);
bool operator!=(const FrameInfo& lhs, const FrameInfo& rhs);
std::ostream& operator<<(std::ostream& out, const FrameInfo& fi);

#endif // MP4_H
