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

#include <assert.h>
#include <string>
#include <iostream>
#include <iomanip>  // setprecision
#include <algorithm>

extern "C" {
#include <stdint.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

#include "mp4.h"
#include "atom.h"
#include "file.h"

using namespace std;

uint Mp4::step_ = 1;

Mp4::Mp4(): root_atom_(NULL) { }

Mp4::~Mp4() {
	delete root_atom_;
}

void Mp4::parseOk(string& filename) {
	filename_ok_ = filename;
	FileRead file(filename);

	logg(I, "parsing healthy moov atom ... \n");
	root_atom_ = new Atom;
	while (true) {
		Atom *atom = new Atom;
		atom->parse(file);
		root_atom_->children_.push_back(atom);
		if(file.atEnd()) break;
	}

	if(root_atom_->atomByName("ctts"))
		cerr << "Composition time offset atom found. Out of order samples possible." << endl;

	if(root_atom_->atomByName("sdtp"))
		cerr << "Sample dependency flag atom found. I and P frames might need to recover that info." << endl;

	Atom *mvhd = root_atom_->atomByName("mvhd");
	if(!mvhd)
		throw "Missing movie header atom";
	timescale_ = mvhd->readInt(12);
//	duration_ = mvhd->readInt(16);  // this value is used nowhere

	// https://github.com/FFmpeg/FFmpeg/blob/70d25268c21cbee5f08304da95be1f647c630c15/doc/APIchanges#L86
    #if ( LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58,9,100) )
	av_register_all();
    #endif

	unmute(); // sets AV_LOG_LEVEL
	context_ = avformat_alloc_context();
	// Open video file
	int error = avformat_open_input(&context_, filename.c_str(), NULL, NULL);

	if(error != 0)
		throw "Could not parse AV file (" + to_string(error) + "): " + filename;

	if(avformat_find_stream_info(context_, NULL) < 0)
		throw "Could not find stream info";

	av_dump_format(context_, 0, filename.c_str(), 0);

	parseTracksOk();

//	if (g_show_tracks) return;  // show original track order

	// reduce false positives when matching
	map<string, int> certainty = {
	    {"gpmd", 4},
	    {"fdsc", 3},
	    {"mp4a", 2},
	    {"avc1", 1},
	    // default is 0
	};
	sort(tracks_.begin(), tracks_.end(),  [&](const Track& a, const Track& b) -> bool {
		return certainty[a.codec_.name_] > certainty[b.codec_.name_];
	});

	if (g_log_mode >= LogMode::I) cout << '\n';
}

void Mp4::printMediaInfo() {
	cout << "tracks:\n";
	for (uint i=0; i < tracks_.size(); i++) {
		auto& track = tracks_[i];
		cout << "  [" << i << "] " << track.handler_type_ << " by '" << track.handler_name_ << "' ";
		if (track.codec_.name_.size())
			cout << "(" << track.codec_.name_ << ") ";
		auto codec_type = av_get_media_type_string(track.codec_.av_codec_params_->codec_type);
		auto codec_name = avcodec_get_name(track.codec_.av_codec_params_->codec_id);
		cout << ss("<", codec_type, ", ", codec_name, ">\n");
	}
}

void Mp4::printAtoms() {
	if (!root_atom_) return;
	for (auto& c : root_atom_->children_)
		c->print(0);
}

void Mp4::makeStreamable(string& filename, string& output) {
	Atom *root = new Atom;
	{
		FileRead file(filename);

		while(1) {
			Atom *atom = new Atom;
			atom->parse(file);
			cout << "Found atom: " << atom->name_ << endl;
			root->children_.push_back(atom);
			if(file.atEnd()) break;
		}
	}
	Atom *ftyp = root->atomByName("ftyp");
	Atom *moov = root->atomByName("moov");
	Atom *mdat = root->atomByName("mdat");

	if(mdat->start_ > moov->start_) {
		cout << "File is already streamable" << endl;
		return;
	}
	int old_start = (mdat->start_ + 8);

	int new_start = moov->length_+8;
	if(ftyp)
		new_start += ftyp->length_ + 8;

	int diff = new_start - old_start;
	cout << "OLD: " << old_start << " new: " << new_start << endl;
	/* MIGHT HAVE TO FIX THIS ONE TOO?
	Atom *co64 = trak->atomByName("co64");
	if(co64) {
		trak->prune("co64");
		Atom *stbl = trak->atomByName("stbl");
		Atom *new_stco = new Atom;
		memcpy(new_stco->name, "stco", 5);
		stbl->children.push_back(new_stco);
	} */
	std::vector<Atom *> stcos = moov->atomsByName("stco");
	for(uint i = 0; i < stcos.size(); i++) {
		Atom *stco = stcos[i];
		uint offsets = stco->readInt(4);   //4 version, 4 number of entries, 4 entries
		for(uint i = 0; i < offsets; i++) {
			int pos = 8 + 4*i;
			int offset = stco->readInt(pos) + diff;
			cout << "O: " << offset << endl;
			stco->writeInt(offset, pos);
		}
	}

	{
		//save to file
		FileWrite file;
		if(!file.create(output))
			throw "Could not create file for writing: " + output;

		if(ftyp)
			ftyp->write(file);
		moov->write(file);
		mdat->write(file);
	}
}

void Mp4::chkStrechFactor() {
	int video = 0, sound = 0;
	for(Track& track : tracks_) {
		int msec = track.getDurationInMs();
		if (track.handler_type_ == "vide") video = msec;
		else if (track.handler_type_ == "soun") sound = msec;
	}
	if (!sound) return;

	double factor = (double) sound / video;
	double eps = 0.1;
	if (fabs(factor - 1) > eps) {
		if (!g_stretch_video)
			cout << "Tip: Audio and video seem to have different durations (" << factor << ").\n"
			     << "     If audio and video are not in sync, give `-sv` a try. See `--help`\n";
		else
			for(Track& track : tracks_) {
				if (track.handler_type_ == "vide") {
					track.stretch_factor_ = (double)sound / video;
//					track.stretch_factor_ = 0.5;
					logg(I, "changing video speed by factor " , 1/factor, "\n");
					track.duration_ *= factor;
					break;
				}
			}
	}

}

void Mp4::setDuration() {
	for(Track& track : tracks_) {
		if (contains(ignore_duration_, track.codec_.name_)) continue;
		duration_ = max(duration_, track.getDurationInTimescale());
	}
}

void Mp4::saveVideo(const string& filename) {
	if (g_dont_write) return;
	/* we save all atom except:
	  ctts: composition offset ( we use sample to time)
	  cslg: because it is used only when ctts is present
	  stps: partial sync, same as sync

	  movie is made by ftyp, moov, mdat (we need to know mdat begin, for absolute offsets)
	  assumes offsets in stco are absolute and so to find the relative just subtrack mdat->start + 8
*/


	chkStrechFactor();
	setDuration();

	for(Track& track : tracks_) {
		Atom *stbl = track.trak_->atomByName("stbl");
		if (broken_is_64_ && stbl->atomByName("stco")){
			stbl->prune("stco");
			Atom *new_co64 = new Atom;
			new_co64->name_ = "co64";
			stbl->children_.push_back(new_co64);
		}
		else if (!broken_is_64_ && stbl->atomByName("co64")) {
			stbl->prune("co64");
			Atom *new_stco = new Atom;
			new_stco->name_ = "stco";
			stbl->children_.push_back(new_stco);
		}
		track.writeToAtoms();

		auto& cn = track.codec_.name_;
		if (contains(ignore_duration_, cn)) continue;

		int hour, min, sec, msec;
		int bmsec = track.getDurationInMs();

		auto x = div(bmsec, 1000); msec = x.rem; sec = x.quot;
		x = div(sec, 60); sec = x.rem; min = x.quot;
		x = div(min, 60); min = x.rem; hour = x.quot;
		string s_msec = (msec?to_string(msec)+"ms ":"");
		string s_sec = (sec?to_string(sec)+"s ":"");
		string s_min = (min?to_string(min)+"min ":"");
		string s_hour = (hour?to_string(hour)+"h ":"");
		logg(I, "Duration of ", cn, ": ", s_hour, s_min, s_sec, s_msec, " (", bmsec, " ms)\n");
	}

	Atom *mdat = root_atom_->atomByName("mdat");

	if (unknown_lengths_.size()) {
		cout << setprecision(4);
		int64_t bytes_not_matched = 0;
		for (auto n : unknown_lengths_) bytes_not_matched += n;
		double percentage = (double)100 * bytes_not_matched / mdat->contentSize();
		logg(W, "Unknown sequences: ", unknown_lengths_.size(), '\n');
		logg(W, "Bytes NOT matched: ", pretty_bytes(bytes_not_matched), " (", percentage, "%)\n");
	}

	logg(I, "saving ", filename, '\n');
	Atom *mvhd = root_atom_->atomByName("mvhd");
	mvhd->writeInt(duration_, 16);


	Atom *ftyp = root_atom_->atomByName("ftyp");
	Atom *moov = root_atom_->atomByName("moov");

	moov->prune("ctts");
	moov->prune("cslg");
	moov->prune("stps");

	root_atom_->updateLength();

	//fix offsets
	int64_t offset = 8 + moov->length_;
	if(ftyp)
		offset += ftyp->length_; //not all mov have a ftyp.

	for (Track& track : tracks_) {
		for(uint i = 0; i < track.offsets_.size(); i++)
			track.offsets_[i] += offset;

		track.saveChunkOffsets(); //need to save the offsets back to the atoms
	}

	//save to file
	FileWrite file;
	if(!file.create(filename))
		throw "Could not create file for writing: " + filename;

	if(ftyp)
		ftyp->write(file);
	moov->write(file);
	mdat->write(file);
}

void Mp4::chkUntrunc(FrameInfo& fi, Codec& c, int i) {
	auto offset = fi.offset_;
	auto& mdat = current_mdat_;
	off64_t real_off = offset + mdat->file_begin_;

	auto start = loadFragment(offset);

	cout << "\n(" << i << ") Size: " << fi.length_ << " offset " << real_off
	     << "  begin: " << mkHexStr(start, 4) << " " << mkHexStr(start+4, 4)
	     << " end: " << mkHexStr(start+fi.length_-4, 8) << '\n';

	bool ok = true;
	bool matches = false;
	for (const auto& t : tracks_)
		if (t.codec_.matchSample(start)){
			if (t.codec_.name_ != c.name_){
				cout << "Matched wrong codec! '" << t.codec_.name_ << "' instead of '" << c.name_ << "'\n";
				ok = false;
			} else {matches = true; break;}
		}
	uint size = c.getSize(start, current_maxlength_);
	uint duration = c.audio_duration_;
	//TODO check if duration is working with the stts duration.

	if(!matches) {
		cout << "Match failed! '" << c.name_ << "' itself not detected\n";
		ok = false;
	}

	cout << "detected size: " << size << " true: " << fi.length_;
	if (size != fi.length_) cout << "  <- WRONG";
	cout << '\n';

	if (c.name_ == "mp4a") {
		cout << "detected duration: " << duration << " true: " << fi.length_;
		if (duration != fi.audio_duration_) cout << "  <- WRONG";
		cout << '\n';
		if (c.was_bad_) cout << "detected bad frame\n";
	}

	if (fi.keyframe_){
		cout << "detected keyframe: " << c.was_keyframe_  << " true: 1\n";
		if (!c.was_keyframe_){
			cout << "keyframe not detected!";
			ok = false;
		}
	}

	if (!ok) hitEnterToContinue();
	//assert(length == track.sizes[i]);

}

void Mp4::analyze(bool gen_off_map) {
	FileRead file;
	if (!current_mdat_) {
		file.open(filename_ok_);
		findMdat(file);
	}
	auto& mdat = current_mdat_;

	for(uint idx=0; idx < tracks_.size(); idx++) {
		Track& track = tracks_[idx];
		Codec& c = track.codec_;
		if (!gen_off_map) cout << "\nTrack " << idx << " codec: " << c.name_ << endl;

		uint k = track.keyframes_.size() ? track.keyframes_[0] : -1, ik = 0;
		for(uint i = 0; i < track.offsets_.size(); i++) {
			bool is_keyframe = k == i;

			if (is_keyframe && ++ik < track.keyframes_.size())
				k = track.keyframes_[ik];

			off64_t off = track.offsets_[i] - (mdat->start_ + 8);
			auto fi = FrameInfo(idx, is_keyframe, track.times_[i], off, track.sizes_[i]);

			if (gen_off_map) off_to_info_[off] = fi;
			else chkUntrunc(fi, c, i);
		}
	}

}

void Mp4::parseTracksOk() {
	Atom *mdat = root_atom_->atomByName("mdat");
	vector<Atom *> traks = root_atom_->atomsByName("trak");
	for (uint i=0; i < traks.size(); i++) {
		tracks_.emplace_back(traks[i], context_->streams[i]->codecpar, timescale_);
		tracks_.back().parse(mdat);
	}
	if (hasCodec("fdsc") &&  hasCodec("avc1"))
		getTrack("avc1").codec_.strictness_level_ = 1;
}

void Mp4::dumpMatch(off64_t off, const FrameInfo& fi, int idx) {
	auto real_off = off + current_mdat_->file_begin_;
//	cout << setw(15) << ss("(", idx++, ") ", mdat_off, "+") << setw(10) << off << " : "  << fi << '\n';
	cout << setw(15) << ss("(", idx++, ") ") << setw(12) << ss(off, " / ") << setw(8) << real_off << " : " << fi << '\n';
}

void Mp4::dumpSamples() {
	analyze(true);
	cout << filename_ok_ << '\n';

	int i = 0;
	for (auto const& x : off_to_info_)
		dumpMatch(x.first, x.second, i++);
}

BufferedAtom* Mp4::findMdat(FileRead& file_read) {
	auto mdat = new BufferedAtom(file_read);
	mdat->name_ = "mdat";
	Atom atom;
	while(atom.name_ != "mdat") {
		off64_t new_pos = Atom::findNextAtomOff(file_read, &atom, true);
		if (new_pos >= file_read.length() || new_pos < 0) {
			logg(W, "start of mdat not found\n");
			atom.start_ = 0;
			file_read.seek(0);
			break;
		}
		file_read.seek(new_pos);
		atom.parseHeader(file_read);
	}
	mdat->start_ = atom.start_;
	mdat->file_begin_ = file_read.pos();
	mdat->file_end_ = file_read.length();

	if (!current_mdat_) current_mdat_ = mdat;
	return mdat;
}

void Mp4::addFrame(FrameInfo& fi) {
	Track& track = tracks_[fi.track_idx_];

	if(fi.keyframe_) {
		track.keyframes_.push_back(track.offsets_.size());
	}
	track.offsets_.push_back(fi.offset_);
	if (fi.audio_duration_) audiotimes_stts_rebuild_.push_back(fi.audio_duration_);

	track.sizes_.push_back(fi.length_);
	track.n_matched++;
}

const uchar* Mp4::loadFragment(off64_t offset, bool be_quiet) {
	if (!be_quiet) logg(V, "\n(reading element from mdat)\n");
	current_maxlength_ = min((int64_t) g_max_partsize, current_mdat_->contentSize() - offset); // max 1.6MB
	auto s = current_fragment_ = current_mdat_->getFragment(offset, current_maxlength_);

	if (g_log_mode >= LogMode::V && !be_quiet) {
		uint begin = swap32(*(uint*)s);
		uint next = swap32(*(uint*)(s+4));
		off64_t real_off = offset + current_mdat_->file_begin_;
		logg(V, "Offset: ", offset, " / ", real_off, " : ", setfill('0'), setw(8), hex, begin, " ", setw(8), next, dec, '\n');
	}

	return s;
}

void Mp4::chkDetectionAt(FrameInfo& detected, off64_t off) {
//	if (g_log_mode < LogMode::V) return;

	auto& correct = off_to_info_[off];
	if (correct != detected) {
		cout << "bad detection (at " << off << " / " << off + current_mdat_->file_begin_ << ", pkt " << pkt_idx_ << "):\n"
		     << "detected: " << detected << '\n'
		     << "correct : " << correct << '\n';
		hitEnterToContinue();
	}
}

bool Mp4::hasCodec(const string& codec_name) {
	for (auto& t : tracks_)
		if (t.codec_.name_ == codec_name) return true;
	return false;
}

uint Mp4::getTrackIdx(const string& codec_name) {
	for (uint i=0; i < tracks_.size(); i++)
		if (tracks_[i].codec_.name_ == codec_name) return i;
	throw "asked for nonexistent track";
}

string Mp4::getCodecName(uint track_idx) {
	if (track_idx >= tracks_.size()) return "????";
	return tracks_[track_idx].codec_.name_;
}

Track& Mp4::getTrack(const string& codec_name) {
	auto idx = getTrackIdx(codec_name);
	return tracks_[idx];
}

bool Mp4::wouldMatch(off64_t offset, const string& skip, bool strict) {
	auto start = loadFragment(offset, true);
	for (Track& track : tracks_) {
		auto& c = track.codec_;
		if (track.codec_.name_ == skip) continue;
		if (strict && !c.matchSampleStrict(start)) continue;
		if (!strict && !c.matchSample(start)) continue;

		logg(V, "wouldMatch(", start, ", ", skip, ", ", strict, " -> yes, ", c.name_, "\n");
		return true;
	}
	logg(V, "wouldMatch(", start, ", ", skip, ", ", strict, " -> no\n");
	return false;
}

FrameInfo Mp4::getMatch(off64_t offset, bool strict) {
	auto start = loadFragment(offset);

	// hardcoded match
	if (pkt_idx_ == 4 && hasCodec("tmcd")) {
		logg(V, "using hardcoded 'tmcd' packet (len=4)");
		if (wouldMatch(offset, "", true))
			logg(W, "Unexpected 4th packet..\n");
		return FrameInfo(getTrackIdx("tmcd"), false, 0, offset, 4);
	}

	for (uint i=0; i < tracks_.size(); i++) {
		auto& track = tracks_[i];
		Codec& c = track.codec_;
		logg(V, "Track codec: ", c.name_, '\n');

		if (strict && !c.matchSampleStrict(start)) continue;
		if (!strict && !c.matchSample(start)) continue;

		int length_signed = c.getSize(start, current_maxlength_);
		uint length = static_cast<uint>(length_signed);

		logg(V, "part-length: ", length_signed, '\n');
		if(length_signed < 1) {
			logg(V, "Invalid length: part-length is ", length_signed, '\n');
			continue;
		}
		if(length > g_max_partsize || length > current_maxlength_) {
			logg(E, "Invalid length: ", length, " - too big (track: ", i, ")\n");
			continue;
		}
		if (c.was_bad_) continue;

		return FrameInfo(i, c, offset, length);
	}

	return FrameInfo();
}

void Mp4::analyzeOffset(const string& filename, off64_t real_offset) {
	FileRead file(filename);
	auto mdat = findMdat(file);
	if (real_offset < mdat->file_begin_ || real_offset >= mdat->file_end_)
		throw "given offset is not in 'mdat'";

	auto buff = file.getPtrAt(real_offset, 16);
	printBuffer(buff, 16);

	auto off = real_offset - mdat->file_begin_;
	auto match = getMatch(off, false);
	dumpMatch(off, match, 0);
}

FrameInfo::FrameInfo(uint track_idx, Codec& c, uint offset, uint length)
    : track_idx_(track_idx), keyframe_(c.was_keyframe_), audio_duration_(c.audio_duration_), offset_(offset), length_(length) {}

FrameInfo::FrameInfo(uint track_idx, bool was_keyframe, uint audio_duration, uint offset, uint length)
    : track_idx_(track_idx), keyframe_(was_keyframe), audio_duration_(audio_duration), offset_(offset), length_(length) {}

FrameInfo::operator bool() {
	return length_;
}

bool operator==(const FrameInfo& a, const FrameInfo& b) {
	return a.length_ == b.length_ && a.track_idx_ == b.track_idx_ &&
	    a.keyframe_ == b.keyframe_;
//	    a.keyframe_ == b.keyframe_ && a.audio_duration_ == b.audio_duration_;

}
bool operator!=(const FrameInfo& a, const FrameInfo& b) { return !(a == b); }

ostream& operator<<(ostream& out, const FrameInfo& fi) {
	if (!fi.length_) return out << "(none)";
	auto cn = g_mp4->getCodecName(fi.track_idx_);
	return out << ss("'", cn, "', ", fi.length_, ", ", fi.keyframe_, ", ", fi.audio_duration_);
}

bool Mp4::chkOffset(off64_t& offset) {
start:
	if (offset >= current_mdat_->contentSize()) {  // at end?
		if(unknown_length_){
			unknown_lengths_.emplace_back(unknown_length_);
			unknown_length_ = 0;
		}
		return false;
	}

	auto start = loadFragment(offset, true);
	uint begin = *(uint*)start;

	static uint loop_cnt = 0;
	if (g_log_mode == I && loop_cnt++ % 2000 == 0) outProgress(offset, current_mdat_->file_end_);

	if (*(int*)start == 0) {
		if (unknown_length_) offset += step_;
		else offset += 4;
		goto start;
	}

	//skip fake moov
	if(start[4] == 'm' && start[5] == 'o' && start[6] == 'o' && start[7] == 'v') {
		logg(V, "Skipping moov atom: ", swap32(begin), '\n');
		offset += swap32(begin);
		goto start;
	}

	return true;
}

void Mp4::repair(string& filename, const string& filename_fixed) {
	FileRead file_read(filename);

	// TODO: What about multiple mdat?

	logg(V, "calling findMdat on truncated file..\n");
	auto mdat = findMdat(file_read);
	logg(I, "reading mdat from truncated file ...\n");

	if (file_read.length() > (1LL<<32)) {
		broken_is_64_ = true;
		logg(I, "using 64-bit offsets for the broken file\n");
	}

	use_offset_map_ = filename == filename_ok_;
	if (use_offset_map_) analyze(true);

	for(uint i=0; i < tracks_.size(); i++)
		tracks_[i].clear();

	//mp4a can be decoded and repors the number of samples (duration in samplerate scale).
	//in some videos the duration (stts) can be variable and we can rebuild them using these values.
	auto& audiotimes = audiotimes_stts_rebuild_;

	off64_t offset = 0;

	while (chkOffset(offset)) {
		auto match = getMatch(offset, unknown_length_);

		if (match) {
			if (use_offset_map_) chkDetectionAt(match, offset);
			addFrame(match);
			offset += match.length_;
			pkt_idx_++;

			if (unknown_length_) {
				unknown_lengths_.emplace_back(unknown_length_);
				unknown_length_ = 0;

				logg(V, "found healthy packet again (", getCodecName(match.track_idx_), "), length: ", match.length_,
				    " duration: ", match.audio_duration_, '\n');
			}
		}
		else if (g_ignore_unknown) {
			if (!g_muted && g_log_mode < LogMode::V && !g_muted) {
				logg(I, "unknown sequence -> muting ffmpeg and warnings ..\n");
				mute();
			}
			unknown_length_ += step_;
			offset += step_;
		}
		else {
			if (g_muted) unmute();
			double percentage = (double)100 * offset / mdat->contentSize();
			mdat->file_end_ = mdat->file_begin_ + offset;
			mdat->length_ = mdat->file_end_ - mdat->file_begin_;

			logg(E, "unable to find correct codec -> premature end", " (~", setprecision(4), percentage, "%)\n",
			"       try '-s' to skip unknown sequences\n\n");
			logg(V, "mdat->file_end: ", mdat->file_end_, '\n');
			break;
		}
	}

	if (g_muted) unmute();

	if (g_log_mode >= I) {
		cout << "Info: Found " << pkt_idx_ << " packets ( ";
		for(const Track& t : tracks_){
			cout << t.codec_.name_ << ": " << t.n_matched << ' ';
			if (t.codec_.name_ == "avc1")
				cout << "avc1-keyframes: " << t.keyframes_.size() << " ";
		}
		cout << ")\n";
	}

	for (auto& track : tracks_) {
		if(audiotimes.size() && audiotimes.size() == track.offsets_.size())
			swap(audiotimes, track.times_);
		track.fixTimes();
	}
	Atom *original_mdat = root_atom_->atomByName("mdat");
	mdat->start_ = original_mdat->start_;
	root_atom_->replace(original_mdat, mdat);

	saveVideo(filename_fixed);
}

