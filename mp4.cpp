//==================================================================//
/*                                                                  *
	Untrunc - mp4.cpp

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

#include "mp4.h"

#include <cassert>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <utility>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}
#include "atom.h"
#include "file.h"


using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::flush;
using std::hex;
using std::min;
using std::move;
using std::string;
using std::swap;
using std::to_string;
using std::vector;


Mp4::Mp4() : root_atom_(NULL) { }

Mp4::~Mp4() {
	delete root_atom_;
}


void Mp4::parseOk(const string& filename) {
	FileRead file;
	bool success = file.open(filename);
	if (!success)
		throw string("Could not open file: ") + filename;
	//auto x = file.read

	logg(I, "parsing healthy moov atom ... \n");
	root_atom_ = new Atom;
	while (true) {
		Atom* atom = new Atom;
		atom->parse(file);
		root_atom_->addChild(move(atom));
		if (file.atEnd())
			break;
	}

	if (root_atom_->find1stAtom("ctts"))
		cerr << "Composition time offset atom found."
			    " Out of order samples possible." << endl;

	if (root_atom_->find1stAtom("sdtp"))
		cerr << "Sample dependency flag atom found."
			    " I and P frames might need to recover that info." << endl;

	Atom* mvhd = root_atom_->find1stAtom("mvhd");
	if (!mvhd)
		throw string("Missing movie header atom");
	timescale_ = mvhd->readInt32(12);
	duration_ = mvhd->readInt32(16);

	av_register_all();
	if (g_log_mode < V)
		av_log_set_level(AV_LOG_WARNING);
	context_ = avformat_alloc_context();
	// Open video file.
	int error = avformat_open_input(&context_, filename.c_str(), NULL, NULL);

	if (error != 0)
		throw "Could not parse AV file: " + filename;

	if (avformat_find_stream_info(context_, NULL) < 0)
		throw string("Could not find stream info");

	av_dump_format(context_, 0, filename.c_str(), 0);

	parseTracks();
}

void Mp4::printMediaInfo() {}

void Mp4::printAtoms() {
	if (root_atom_)
		root_atom_->print(0);
}

void Mp4::makeStreamable(const string& filename, const string& output) {
	Atom* root = new Atom;
	{
		FileRead file;
		bool success = file.open(filename);
		if (!success)
			throw string("Could not open file: ") + filename;

		while (true) {
			Atom* atom = new Atom;
			atom->parse(file);
			cout << "Found atom: " << atom->completeName() << endl;
			root->addChild(move(atom));
			if (file.atEnd()) break;
		}
	}
	Atom* ftyp = root->find1stAtom("ftyp");
	Atom* moov = root->find1stAtom("moov");
	Atom* mdat = root->find1stAtom("mdat");

	if (mdat->contentPos() > moov->contentPos()) {
		cout << "FIle is already streamable" << endl;
		return;
	}

	int old_start = mdat->contentPos();
	int new_start = moov->length() + mdat->headerSize();
	if (ftyp)
		new_start += ftyp->length();

	int diff = new_start - old_start;
	cout << "OLD: " << old_start << " new: " << new_start << endl;
#if 0  // MIGHT HAVE TO FIX THIS ONE TOO?
	Atom* co64 = trak->find1stAtom("co64");
	if (co64) {
		trak->pruneAtoms("co64");
		Atom* stbl = trak->find1stAtom("stbl");
		if (stbl) {
			Atom* new_stco = new Atom("stco");
			stbl->addChild(move(new_stco));
		}
	}
#endif
	std::vector<Atom*> stcos = moov->findAllAtoms("stco");
	for (int i = 0; i < stcos.size(); i++) {
		Atom* stco = stcos[i];
		int offsets = stco->readInt32(4);  // 4 version, 4 number of entries, 4 entries.
		for (unsigned int i = 0; i < offsets; i++) {
			int pos = 8 + 4 * i;
			int offset = stco->readInt32(pos) + diff;
			cout << "O: " << offset << endl;
			stco->writeInt32(pos, offset);
		}
	}

	{
		// Save to file.
		FileWrite file;
		if (!file.create(output))
			throw "Could not create file for writing: " + output;

		if (ftyp)
			ftyp->write(&file);
		moov->write(&file);
		mdat->write(&file);
	}
}

void Mp4::saveVideo(const string& filename) {
	// We save all atom except:
	//  ctts: composition offset (we use sample to time).
	//  cslg: because it is used only when ctts is present.
	//  stps: partial sync; same as sync.
	//
	// Movie is made by ftyp, moov, mdat
	//  (we need to know mdat begin, for absolute offsets).
	// Assume offsets in stco are absolute and
	//  so to find the relative just subtrack mdat->start + 8.

	duration_ = 0;
	for (Track& track : tracks_) {
		track.writeToAtoms();
		// Convert to movie timescale.
#if 0
		cout << "Track duration: " << track.duration_
			 << " movie timescale: " << timescale_
			 << " track timescale: " << track.timescale_ << endl;
#endif
		int track_duration = static_cast<int>(
			static_cast<double>(track.duration_) *
			(static_cast<double>(timescale_) / track.timescale_));
		if (track_duration > duration_)
			duration_ = track_duration;

		int hour, min, sec, msec;
		int bmsec = static_cast<int>(int64_t(track.duration_) * 1000 /
									 track.timescale_);
		auto x = div(bmsec, 1000);
		msec = x.rem;
		sec = x.quot;
		x = div(sec, 60);
		sec = x.rem;
		min = x.quot;
		x = div(min, 60);
		min = x.rem;
		hour = x.quot;
		string s_msec = (msec ? to_string(msec) + "ms " : "");
		string s_sec = (sec ? to_string(sec) + "s " : "");
		string s_min = (min ? to_string(min) + "min " : "");
		string s_hour = (hour ? to_string(hour) + "h " : "");
		logg(I, "Duration of ", track.codec_.name_, ": ",
			 s_hour, s_min, s_sec, s_msec, " (", bmsec, " ms)\n");

		Atom* tkhd = track.trak_->find1stAtom("tkhd");
		// In movie timescale, not track timescale.
		tkhd->writeInt32(20, track_duration);
	}
	logg(I, "saving ", filename, '\n');
	Atom* mvhd = root_atom_->find1stAtom("mvhd");
	mvhd->writeInt32(16, duration_);


	Atom* ftyp = root_atom_->find1stAtom("ftyp");
	Atom* moov = root_atom_->find1stAtom("moov");
	Atom* mdat = root_atom_->find1stAtom("mdat");

	moov->pruneAtoms("ctts");
	moov->pruneAtoms("cslg");
	moov->pruneAtoms("stps");

	root_atom_->updateLength();

	// Fix offsets.
	int offset = 8 + moov->length();
	if (ftyp)
		offset += ftyp->length();  // Not all mov have a ftyp.

	for (unsigned int t = 0; t < tracks_.size(); t++) {
		Track& track = tracks_[t];
		for (unsigned int i = 0; i < track.offsets_.size(); i++)
			track.offsets_[i] += offset;

		track.writeToAtoms();     // Need to save the offsets back to the atoms.
	}

	// save to file
	FileWrite file;
	if (!file.create(filename))
		throw "Could not create file for writing: " + filename;

	if (ftyp)
		ftyp->write(&file);
	moov->write(&file);
	mdat->write(&file);
}

void Mp4::analyze() {
	Atom* mdat = root_atom_->find1stAtom("mdat");
	for (unsigned int i = 0; i < tracks_.size(); i++) {
		cout << "\n\n Track " << i << endl;
		Track& track = tracks_[i];

		cout << "Track codec: " << track.codec_.name_ << endl;

		cout << "Keyframes  " << track.keyframes_.size() << endl;
		for (unsigned int i = 0; i < track.keyframes_.size(); i++) {
			int k = track.keyframes_[i];
			int offset = track.offsets_[k] - mdat->contentPos();
			int begin = mdat->readInt32(offset);
			int next = mdat->readInt32(offset + 4);
			cout << "\n"
				 << k << " Size: " << track.sizes_[k] << " offset " << track.offsets_[k]
				 << "  begin: " << hex << begin << " " << next << dec << endl;
		}

		for (unsigned int i = 0; i < track.offsets_.size(); i++) {
			int offset = track.offsets_[i] - mdat->contentPos();
			uint maxlength = mdat->contentSize() - offset;
			const uchar* start = mdat->content(offset, maxlength);

			int begin = mdat->readInt32(offset);
			int next = mdat->readInt32(offset + 4);
			int end = mdat->readInt32(offset + track.sizes_[i] - 4);
			cout << i << " Size: " << track.sizes_[i]
				 << " offset " << track.offsets_[i]
				 << "  begin: " << hex << begin << " " << next
				 << " end: " << end << dec << endl;

			bool matches = track.codec_.matchSample(start);
			int duration = 0;
			int length = track.codec_.getLength(start, maxlength, duration);
			// TODO(ponchio): Check if duration is working with the stts duration.

			if (!matches) {
				cout << "Match failed! Hit enter for next match." << endl;
				getchar();
			}
			//assert(matches);
			cout << "Length: " << length << " true length: " << track.sizes_[i] << endl;
			if (length != track.sizes_[i])
				getchar();
			//assert(length == track.sizes[i]);
		}
	}
}

void Mp4::writeTracksToAtoms() {
	for (unsigned int i = 0; i < tracks_.size(); i++)
		tracks_[i].writeToAtoms();
}

void Mp4::parseTracks() {
	Atom* mdat = root_atom_->find1stAtom("mdat");
	vector<Atom*> traks = root_atom_->findAllAtoms("trak");
	for (unsigned int i = 0; i < traks.size(); i++) {
		Track track(traks[i], context_->streams[i]->codec);
		track.parse(mdat);
		tracks_.push_back(track);
	}
}

void Mp4::repair(const string& filename, const string& filename_fixed) {
	// Find mdat. Fails with krois and a few other.
	// TODO(ponchio): Check for multiple mdat, or just look for the first one.

	logg(I, "parsing mdat from truncated file ... \n");
	AtomWrite* mdat = new AtomWrite(filename);
	while (true) {
		try {
			mdat->parseHeader();
		} catch (string) {
			delete mdat;
			throw string("Failed to parse atoms in truncated file: ") +
				         filename;
			return;
		}
		if (*mdat == "mdat") break;
		if (!mdat->nextHeader()) {
			delete mdat;
			throw string("Failed to find in truncated file atom: ") +
				         Atom::completeName("mdat");
			return;
		}
	}

	for (unsigned int i = 0; i < tracks_.size(); i++)
		tracks_[i].clear();

	// Mp4a is more reliable than Avc1.
	if (tracks_.size() > 1 && tracks_[1].codec_.name_ == "mp4a")
		swap(tracks_[0], tracks_[1]);

	// Mp4a can be decoded and reports the number of samples
	//  (duration in samplerate scale).
	// In some videos the duration (stts) can be variable and
	//  we can rebuild them using these values.
	vector<int> audiotimes;
	size_t cnt_packets = 0;
	off_t offset = 0;

	logg(V, "mdat->contentSize = ", mdat->contentSize(), '\n');
	int loop_cnt = 0;
	while (offset < mdat->contentSize()) {
		logg(V, "\n(reading element from mdat)\n");
		if (g_log_mode == I) {  // output progress
			if (loop_cnt >= 2000) {
				double p = round(1000.0 * (static_cast<double>(offset) /
							     (mdat->contentPos() + mdat->contentSize())));
				cout << p / 10 << "%  \r" << flush;
				loop_cnt = 0;
			}
			loop_cnt++;
		}

		// Maximal length is 1.6 MB.
		uint maxlength = min(g_max_partsize, mdat->contentSize(offset));
		const uchar* start = mdat->content(offset, maxlength);

		uint begin = mdat->readInt32(offset);

		if (begin == 0) {
#if 0  // AARRGH this sometimes is not very correct, unless it's all zeros.
			// Skip zeros to next 000.
			offset &= 0xfffff000;
			offset += 0x1000;
#else
			offset += 4;
#endif
			continue;
		}

		uint next = mdat->readInt32(offset + 4);

		logg(V, "Offset: ", offset, " ", hex, begin, " ", next, dec, '\n');

		// Skip fake moov.
		if (start[4] == 'm' && start[5] == 'o' &&
			start[6] == 'o' && start[7] == 'v') {
			logg(V, "Skipping moov atom: ", begin, '\n');
			offset += begin;
			continue;
		}

		bool found = false;
		for (unsigned int i = 0; i < tracks_.size(); i++) {
			Track& track = tracks_[i];
			logg(V, "Track codec: ", track.codec_.name_, '\n');
			// Sometime audio packets are difficult to match,
			//  but if they are the only ones...
			int duration = 0;
			if (tracks_.size() > 1 && !track.codec_.matchSample(start))
				continue;
			int length = track.codec_.getLength(start, maxlength, duration);
			logg(V, "frame-length: ", length, '\n');
			if (length < -1 || length > g_max_partsize) {
				logg(E, "Invalid length: ", length, ". Wrong match in track: ", i, '\n');
				continue;
			}
			if (length == -1 || length == 0) {
				logg(V, "Invalid length: part-length is -1 or 0\n");
				continue;
			}
			if (length > maxlength) {
				logg(V, "Invalid length: part-length >= maxlength\n");
				continue;
			}
			if (length > 8)
				logg(V, "- found as ", track.codec_.name_, '\n');

			bool keyframe = track.codec_.last_frame_was_idr_;
			if (keyframe)
				track.keyframes_.push_back(track.offsets_.size());
			track.offsets_.push_back(offset);
			track.sizes_.push_back(length);
			offset += length;
			if (duration)
				audiotimes.push_back(duration);

			found = true;
			track.n_matched++;
			break;
		}

		if (!found) {
			// This could be a problem for large files.
			//assert(mdat->length() == mdat->headerSize() + mdat->contentSize());
			mdat->contentResize(offset);
			if (mdat->headerSize() + mdat->contentSize() != mdat->length()) {
				logg(E, "unable to find correct codec -> premature end\n");
				logg(V, "Atom mdat content end pos: ",
					    mdat->contentPos() + mdat->contentSize(), '\n');
			}
			break;
		}
		cnt_packets++;
	}

	if (g_log_mode >= I) {
		cout << "Info: Found " << cnt_packets << " packets ( ";
		for (const Track& t : tracks_) {
			cout << t.codec_.name_ << ": " << t.n_matched << ' ';
			if (t.codec_.name_ == "avc1")
				cout << "avc1-keyframes: " << t.keyframes_.size() << " ";
		}
		cout << ")\n";
	}

	for (auto& track : tracks_) {
		if (audiotimes.size() == track.offsets_.size())
			swap(audiotimes, track.times_);

		track.fixTimes();
	}

	if (!root_atom_->replaceChild("mdat", move(mdat)))
		throw string("Could not replace atom: ") + Atom::completeName("mdat");

	saveVideo(filename_fixed);
}


// vim:set ts=4 sw=4 sts=4 noet:
