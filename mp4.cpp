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
#include <iomanip>
#include <iostream>
#include <limits>
#include <utility>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/log.h"
}  // extern "C"
#include "atom.h"
#include "common.h"
#include "file.h"


using std::cerr;
using std::cin;
using std::clog;
using std::cout;
using std::dec;
using std::exchange;
using std::flush;
using std::hex;
using std::min;
using std::move;
using std::setw;
using std::string;
using std::swap;
using std::vector;


Mp4::~Mp4() {
	close();
}

void Mp4::close() {
	Atom* rm_root = exchange(root_atom_, nullptr);
	timescale_ = 0;
	duration_  = 0;
	tracks_.clear();  // Must clear tracks before closing context.
	if (context_) {
		AvLog useAvLog(AV_LOG_ERROR);
		avformat_close_input(&context_);
		context_ = nullptr;
	}
	filename_.clear();
	delete rm_root;
}


void Mp4::parseOk(const string& filename) {
	logg(I, "Opening: ", filename, ".\n");
	close();

	{  // Parse ok file.
		FileRead file(filename);
		root_atom_ = new Atom;
		while (!file.atEnd()) {
			Atom* atom = new Atom;
			atom->parse(file);
			logg(VV, "Found atom: ", atom->completeName(), '\n');
			root_atom_->addChild(move(atom));
		}
	}  // {
	filename_ = filename;

	if (root_atom_->find1stAtom("ctts")) {
		logg(W, "Found atom: ", Atom::completeName("ctts"),
			    ". Out of order samples possible.\n");
	}
	if (root_atom_->find1stAtom("sdtp")) {
		logg(W, "Found atom: ", Atom::completeName("sdtp"),
			    ". I and P frames might need to recover that info.\n");
	}

	Atom* mvhd = root_atom_->find1stAtom("mvhd");
	if (!mvhd) {
		logg(E, "Missing atom: ", Atom::completeName("mvhd"), ".\n");
		throw string("Missing atom: ") + Atom::completeName("mvhd");
		return;
	}
	// ASSUME: mvhd atom version 0.
	timescale_ = mvhd->readUint32(12);
	duration_  = mvhd->readUint32(16);
	if (timescale_ == 0)
		logg(W, "No time scale in atom: ", mvhd->completeName());

	{  // Setup AV library.
		if (g_log_mode < V) av_log_set_level(AV_LOG_WARNING);
		AvLog useAvLog(AV_LOG_WARNING);
		// Register all formats and codecs.
		av_register_all();
		context_ = avformat_alloc_context();
		// Open video file.
		int error = avformat_open_input(&context_, filename.c_str(), NULL, NULL);
		if (error != 0) {
			logg(E, "Could not parse AV file: ", filename, ".\n");
			throw string("Could not parse AV file: ") + filename;
			return;
		}
		// Retrieve stream information.
		if (avformat_find_stream_info(context_, NULL) < 0) {
			logg(E, "Could not find stream info.\n");
			throw string("Could not find stream info");
			return;
		}
	}  // {

	parseTracks();
}


void Mp4::printMediaInfo() {
	if (context_) {
		cout.flush();
		clog.flush();
		cout << "Media Info:\n"
			 << "  Default stream: "
			 << av_find_default_stream_index(context_) << '\n';
		AvLog useAvLog(AV_LOG_INFO);
		FileRedirect redirect(stderr, stdout);
		av_dump_format(context_, 0, filename_.c_str(), 0);
	}
}

void Mp4::printAtoms() {
	if (root_atom_) {
		cout << "Atoms:\n";
		root_atom_->print();
	}
}


bool Mp4::makeStreamable(const string& filename, const string& filename_fixed) {
	logg(I, "Make Streamable: ", filename, ".\n");

	Atom atom_root;
	{  // Parse input file.
		FileRead file(filename);

		while (!file.atEnd()) {
			Atom* atom = new Atom;
			atom->parse(file);
			logg(VV, "Found atom: ", atom->completeName(), '\n');
			atom_root.addChild(move(atom));
		}
	}  // {

	Atom* ftyp = atom_root.find1stAtom("ftyp");
	Atom* moov = atom_root.find1stAtom("moov");
	Atom* mdat = atom_root.find1stAtom("mdat");
	if (!moov || !mdat) {
		if (!moov) logg(E, "Missing atom: ", Atom::completeName("moov"), ".\n");
		if (!mdat) logg(E, "Missing atom: ", Atom::completeName("mdat"), ".\n");
		return false;
	}

	if (mdat->contentPos() > moov->contentPos()) {
		logg(I, "File is already streamable.\n");
		return true;
	}

	// Adjust chunk offset positions.
	off_t old_start = mdat->contentPos();
	off_t new_start = moov->length() + mdat->headerSize();
	if (ftyp) new_start += ftyp->length();  // Not all .mov have an ftyp.
	off_t diff = new_start - old_start;
	logg(V, "Chunk offsets: Old: ", old_start, " -> new: ", new_start, '\n');
#if 0  // MIGHT HAVE TO FIX THIS ONE TOO?
	Atom* co64 = trak->find1stAtom("co64");
	if (co64) {
		trak->pruneAtoms("co64");
		Atom* stbl = trak->find1stAtom("stbl");
		if (stbl && !stbl->findChild("stco")) {
			Atom* new_stco = new Atom("stco");
			stbl->addChild(move(new_stco));
		}
	}
#endif
	std::vector<Atom*> stcos = moov->findAllAtoms("stco");
	for (auto& stco : stcos) {
		// stco: 4 vers+flags, 4 num-entries, 4* chunk-pos.
		uint32_t entries = stco->readUint32(4);
		for (unsigned int i = 0; i < entries; ++i) {
			int64_t  offset    = 8 + 4 * i;  // Atom content offset.
			uint32_t chunk_pos = stco->readUint32(offset);  // File position.
			logg(VV, "Chunk offset: ", chunk_pos, " -> ", (chunk_pos + diff),
				     "\n");
			chunk_pos += diff;
			stco->writeUint32(offset, chunk_pos);
		}
	}

	{  // Save to fixed file.
		logg(I, "Saving to: ", filename_fixed, ".\n");
		FileWrite file(filename_fixed);

		if (ftyp) ftyp->write(&file);
		moov->write(&file);
		mdat->write(&file);
	}  // {
	logg(I);
	return true;
}


bool Mp4::save(const string& filename) {
	// We save all atoms except:
	//  ctts: composition offset (we use sample to time).
	//  cslg: because it is used only when ctts is present.
	//  stps: partial sync; same as sync.
	//
	// Movie is made by ftyp, moovi and mdat
	//  (we need to know mdat begin, for absolute offsets).
	// Assume offsets in stco are absolute and
	//  so to find the relative just subtrack mdat->contentPos().

	logg(I, "Saving to: ", filename, ".\n");
	if (!root_atom_) {
		logg(E, "No file opened.\n");
		return false;
	}

	if (timescale_ == 0) {
		timescale_ = 600;  // Default movie time scale.
		logg(W, "Using new movie time scale: ", timescale_, ".\n");
	}
	duration_ = 0;
	for (Track& track : tracks_) {
		logg(I, "Track codec: ", track.codec_.name_, "  duration: ",
			    track.duration_, " timescale: ", track.timescale_, " (",
				duration2String(track.duration_, track.timescale_), ").\n");
		if (track.timescale_ == 0 && track.duration_ != 0)
			logg(W, "Track codec ", track.codec_.name_, " has no time scale.\n");

		track.writeToAtoms();

		// Convert duration to movie timescale.
		if (track.duration_ == 0) continue;  // Nothing to convert.
#if 0
		// Use default movie time scale if no track time scale was found.
		uint32_t track_timescale = (track.timescale_ != 0)
								   ? track.timescale_ : 600U;
		uint32_t track_duration  = static_cast<uint32_t>(
			static_cast<double>(track.duration_) * timescale_ /
			track_timescale);
#elif 1
		// Use default movie time scale if no track time scale was found.
		uint32_t ttimescale = (track.timescale_ != 0) ? track.timescale_ : 600U;
		uint64_t tduration  = track.duration_;
		tduration = (tduration * timescale_ + ttimescale - 1) / ttimescale;
		if (tduration > std::numeric_limits<uint32_t>::max()) {
			logg(E, "New track duration too large (", tduration, " -> ",
					std::numeric_limits<uint32_t>::max(), ").\n");
			tduration = std::numeric_limits<uint32_t>::max();
		}
		uint32_t track_duration = static_cast<uint32_t>(tduration);
#endif

		if (duration_ < track_duration)
			duration_ = track_duration;

		Atom* tkhd = track.trak_->find1stAtom("tkhd");
		if (!tkhd) {
			logg(E, "Missing atom: ", Atom::completeName("tkhd"), ".\n");
			continue;
		}
		if (tkhd->readUint32(20) == track_duration) continue;
		logg(I, "Adjust track duration to movie time scale: New duration: ",
			    track_duration, " timescale: ", timescale_,
				" (", duration2String(track_duration, timescale_), ").\n");
		// In movie timescale, not track timescale.
		tkhd->writeUint32(20, track_duration);
	}

	logg(I, "Movie duration: ", duration_, " timescale: ", timescale_,
		    " (", duration2String(duration_, timescale_), ").\n");
	Atom* mvhd = root_atom_->find1stAtom("mvhd");
	if (!mvhd) {
		logg(E, "Missing atom: ", Atom::completeName("mvhd"), ".\n");
		throw string("Missing atom: ") + Atom::completeName("mvhd");
		return false;
	}
	mvhd->writeUint32(16, duration_);

	Atom* ftyp = root_atom_->find1stAtom("ftyp");
	Atom* moov = root_atom_->find1stAtom("moov");
	Atom* mdat = root_atom_->find1stAtom("mdat");
	if (!moov || !mdat) {
		if (!moov) logg(E, "Missing atom: ", Atom::completeName("moov"), ".\n");
		if (!mdat) logg(E, "Missing atom: ", Atom::completeName("mdat"), ".\n");
		return false;
	}

	moov->pruneAtoms("ctts");
	moov->pruneAtoms("cslg");
	moov->pruneAtoms("stps");

	root_atom_->updateLength();

	// Fix offsets.
	uint64_t pos = moov->length() + mdat->headerSize();
	if (ftyp) pos += ftyp->length();  // Not all .mov have an ftyp.
	for (Track& track : tracks_) {
		for (auto& track_offs : track.offsets_) {
			track_offs += pos;
		}
		track.writeToAtoms();  // Need to save the offsets back to the atoms.
	}

	{  // Save to output file.
		logg(I, "Saving to ", filename, ".\n");
		FileWrite file(filename);

		if (ftyp) ftyp->write(&file);
		moov->write(&file);
		mdat->write(&file);
	}  // {
	logg(I);
	return true;
}


void Mp4::analyze(bool interactive) {
	cout << "Analyze:\n";
	if (!root_atom_) {
		cout << "No file opened.\n";
		return;
	}

	Atom* mdat = root_atom_->find1stAtom("mdat");
	if (!mdat) {
		cout << "Missing atom: " << Atom::completeName("mdat") << ".\n";
		return;
	}
	if (interactive) {
		// For interactive analyzis, std::cin & std::cout must both
		// be connected to a terminal/tty.
		if (!isATerminal(std::cin)) {
			clog << "Cannot analyze interactively "
					"as input doesn't come directly from a terminal.\n";
			interactive = false;
		}
		if (interactive && !isATerminal(std::cout)) {
			clog << "Cannot analyze interactively "
					"as output doesn't go directly to a terminal.\n";
			interactive = false;
		}
		if (interactive) {
			// Reset state: clear transient errors of previous input operations.
			cin.clear();
		}
		clog.flush();
	}

	for (unsigned int i = 0; i < tracks_.size(); ++i) {
		cout << "\n\nTrack " << i << '\n';
		Track& track = tracks_[i];
		cout << "Track codec: " << track.codec_.name_ << '\n';
		cout << "Keyframes  : " << track.keyframes_.size() << '\n';
		for (auto k : track.keyframes_) {
			int64_t  offset = track.offsets_[k] - mdat->contentPos();
			uint32_t begin  = mdat->readUint32(offset);
			uint32_t next   = mdat->readUint32(offset + 4);
			cout << setw(8) << k
				 << " Size: " << setw(6) << track.sizes_[k]
				 << " offset " << setw(10) << track.offsets_[k]
				 << "  begin: " << hex << setw(5) << begin << ' '
				 << setw(8) << next << dec << '\n';
		}

		for (unsigned int i = 0; i < track.offsets_.size(); ++i) {
			int64_t  offset = track.offsets_[i] - mdat->contentPos();
			uint64_t maxlength64 = mdat->contentSize() - offset;
			const uchar* start = mdat->content(offset, maxlength64);
			uint32_t maxlength = min(maxlength64, kMaxPartSize);

			uint32_t begin = mdat->readUint32(offset);
			uint32_t next  = mdat->readUint32(offset + 4);
			uint32_t end   = mdat->readUint32(offset + track.sizes_[i] - 4);
			cout << "\n\n>" << setw(7) << i
				 << " Size: " << setw(6) << track.sizes_[i]
				 << " offset " << setw(10) << track.offsets_[i]
				 << "  begin: " << hex << setw(5) << begin << ' '
				 << setw(8) << next
				 << " end: " << setw(8) << end << dec << '\n';

			cout.flush();  // Flush here untill track code is fixed.
			//bool matches = track.codec_.matchSample(start, maxlength);
			bool matches = track.codec_.matchSample(start);
			int duration = 0;
			int length = track.codec_.getLength(start, maxlength, duration);
			// TODO(ponchio): Check if duration is working
			//                with the stts duration.
			cout << "Length: " << length
				 << " true-length: " << track.sizes_[i] << '\n';

			bool wait = false;
			if (!matches) {
				cerr << "- Match failed!\n";
				wait = interactive;
			}
			if (length != track.sizes_[i]) {
				cerr << "- Length mismatch!\n";
				wait = interactive;
			}
			if (length < -1 || length > kMaxPartSize) {
				cerr << "- Invalid length!\n";
				wait = interactive;
			}
			if (wait) {
				// cout and clog have already been flushed by cerr.
				cout << "  <Press [Enter] for next match>\r";
				cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			}
			//assert(matches);
			//assert(length == track.sizes_[i]);
		}
	}
	cout << std::endl;
}

void Mp4::writeTracksToAtoms() {
	for (Track& track : tracks_)
		track.writeToAtoms();
}

bool Mp4::parseTracks() {
	assert(root_atom_);

	Atom* mdat = root_atom_->find1stAtom("mdat");
	if (!mdat) {
		logg(E, "Missing atom: ", Atom::completeName("mdat"), ".\n");
		return false;
	}
	vector<Atom*> traks = root_atom_->findAllAtoms("trak");
	for (unsigned int i = 0; i < traks.size(); ++i) {
		Track track(traks[i], context_->streams[i]->codec);
		track.parse(mdat);
		tracks_.push_back(track);
	}
	return true;
}


bool Mp4::repair(const string& filename_corrupt, const string& filename_fixed) {
	logg(I, "Repair: ", filename_corrupt, ".\n");

	AtomWrite* mdat = new AtomWrite(filename_corrupt);
	logg(I, "Parsing the truncated file for atom: ",
		    Atom::completeName("mdat"), ".\n");
	// Find mdat. Fails with krois and a few other.
	// TODO(ponchio): Check for multiple mdat, or just look for the first one.
	while (true) {
		try {
			mdat->parseHeader();
		} catch (string) {
			delete mdat;
			logg(E, "Failed to parse atoms in truncated file ",
				    filename_corrupt, ".\n");
			throw string("Failed to parse atoms in truncated file: ") +
				         filename_corrupt;
			return false;
		}
		if (*mdat == "mdat") break;  // Found it.
		if (!mdat->nextHeader()) {
			delete mdat;
			logg(E, "Failed to find in truncated file the atom ",
				    Atom::completeName("mdat"), ".\n");
			throw string("Failed to find in truncated file atom: ") +
				         Atom::completeName("mdat");
			return false;
		}
	}

	for (Track& track : tracks_)
		track.clear();

	// Mp4a is more reliable than Avc1.
	if (tracks_.size() > 1 && tracks_[0].codec_.name_ != "mp4a" &&
		tracks_[1].codec_.name_ == "mp4a") {
		logg(I, "Swapping tracks: ", tracks_[0].codec_.name_, " <-> mp4a\n");
		swap(tracks_[0], tracks_[1]);
	}

	// Mp4a can be decoded and reports the number of samples
	//  (duration in sample-rate scale).
	// In some videos the duration (stts) can be variable and
	//  we can rebuild them using these values.
	vector<int> audiotimes;
	size_t cnt_packets = 0;
	off_t  offset      = 0;
	int    loop_cnt    = 0;
	logg(V, "mdat->contentSize = ", mdat->contentSize(), '\n');
	while (offset < mdat->contentSize()) {
		logg(V, "\n(reading element from mdat)\n");
		if (g_log_mode == I) {  // Output progress.
			if (loop_cnt >= 2000) {
				double p = round(1000.0 * (static_cast<double>(offset) /
							     mdat->contentSize()));
				cout << p / 10 << "%  \r" << flush;
				loop_cnt = 0;
			}
			loop_cnt++;
		}

		// Maximal length is 1.6 MB.
		uint32_t maxlength = min(mdat->contentSize(offset), kMaxPartSize);
		const uchar* start = mdat->content(offset, maxlength);

		uint32_t begin = mdat->readUint32(offset);
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
		uint32_t next = mdat->readUint32(offset + 4);
		logg(V, "Offset: ", setw(10), offset,
			    "  begin: ", hex, setw(5), begin,
				' ', setw(8), next, dec, '\n');

		// Skip fake moov.
		if (next == name2Id("moov")) {
			logg(V, "Skipping atom moov (length: ", begin, ").\n");
			offset += begin;
			continue;
		}

		bool found = false;
		for (unsigned int i = 0; i < tracks_.size(); ++i) {
			Track& track = tracks_[i];
			logg(V, "Track codec: ", track.codec_.name_, '\n');
			// Sometime audio packets are difficult to match,
			//  but if they are the only ones...
			if (tracks_.size() > 1 && !track.codec_.matchSample(start))
				continue;
			int duration = 0;
			int length = track.codec_.getLength(start, maxlength, duration);
			logg(V, "frame-length: ", length, '\n');
			if (length < -1 || length > kMaxPartSize) {
				logg(E, "Invalid length: ", length,
					    ". Wrong match in track ", i, ".\n");
				continue;
			}
			if (length == -1 || length == 0) {
				logg(V, "Invalid part-length: ", length, ".\n");
				continue;
			}
			if (length > maxlength) {
				logg(V, "Invalid part-length: ", length, " > ", maxlength,
					    ".\n");
				continue;
			}
			if (length > 8)
				logg(V, "Found: ", track.codec_.name_, ".\n");

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
		logg(V);

		if (!found) {
			// This could be a problem for large files.
			//assert(mdat->length() == mdat->headerSize() + mdat->contentSize());
			mdat->contentResize(offset);
			if (mdat->headerSize() + mdat->contentSize() != mdat->length()) {
				logg(E, "Unable to find correct codec -> premature end.\n");
				logg(V, "Atom mdat content end pos: ",
					    mdat->contentPos() + mdat->contentSize(), ".\n");
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

	return save(filename_fixed);
}


// vim:set ts=4 sw=4 sts=4 noet:
