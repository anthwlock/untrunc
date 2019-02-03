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

extern "C" {
#include <stdint.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

#include "mp4.h"
#include "atom.h"
#include "file.h"

using namespace std;

Mp4::Mp4(): root_atom_(NULL) { }

Mp4::~Mp4() {
	delete root_atom_;
}


void Mp4::parseOk(string& filename) {
	FileRead file;
	bool success = file.open(filename);
	if(!success) throw string("Could not open file: ") + filename;
	//    auto x = file.read

	logg(I, "parsing healthy moov atom ... \n");
	root_atom_ = new Atom;
	while(1) {
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
		throw string("Missing movie header atom");
	timescale_ = mvhd->readInt(12);
	duration_ = mvhd->readInt(16);

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
		throw string("Could not find stream info");

	av_dump_format(context_, 0, filename.c_str(), 0);

	parseTracks();
	//mp4a matching gives less false positives
	if(tracks_.size() > 1 && tracks_[1].codec_.name_ == "mp4a")
		swap(tracks_[0], tracks_[1]);
}

void Mp4::printMediaInfo() {

}

void Mp4::printAtoms() {
	if(root_atom_)
		root_atom_->print(0);
}

void Mp4::makeStreamable(string& filename, string& output) {
	Atom *root = new Atom;
	{
		FileRead file;
		bool success = file.open(filename);
		if(!success) throw string("Could not open file: ") + filename;

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

void Mp4::saveVideo(const string& filename) {
	/* we save all atom except:
	  ctts: composition offset ( we use sample to time)
	  cslg: because it is used only when ctts is present
	  stps: partial sync, same as sync

	  movie is made by ftyp, moov, mdat (we need to know mdat begin, for absolute offsets)
	  assumes offsets in stco are absolute and so to find the relative just subtrack mdat->start + 8
*/

//	duration_ = 0;
	for(Track& track : tracks_) {
		Atom *stbl = track.trak_->atomByName("stbl");
		if (broken_is_64_ && stbl->atomByName("stco")){
			stbl->prune("stco");
			Atom *new_co64 = new Atom;
			memcpy(new_co64->name_, "co64", 5);
			stbl->children_.push_back(new_co64);
		}
		else if (!broken_is_64_ && stbl->atomByName("co64")) {
			stbl->prune("co64");
			Atom *new_stco = new Atom;
			memcpy(new_stco->name_, "stco", 5);
			stbl->children_.push_back(new_stco);
		}
		track.writeToAtoms();
		Atom *tkhd = track.trak_->atomByName("tkhd");
		tkhd->writeInt(track.duration_in_timescale_, 20); //in movie timescale, not track timescale

		int hour, min, sec, msec;
		int bmsec = track.getDurationInMs();
		auto x = div(bmsec, 1000); msec = x.rem; sec = x.quot;
		x = div(sec, 60); sec = x.rem; min = x.quot;
		x = div(min, 60); min = x.rem; hour = x.quot;
		string s_msec = (msec?to_string(msec)+"ms ":"");
		string s_sec = (sec?to_string(sec)+"s ":"");
		string s_min = (min?to_string(min)+"min ":"");
		string s_hour = (hour?to_string(hour)+"h ":"");
		logg(I, "Duration of ", track.codec_.name_, ": ", s_hour, s_min, s_sec, s_msec, " (", bmsec, " ms)\n");
	}

	Atom *mdat = root_atom_->atomByName("mdat");

	if (unknown_lengths_.size()) {
		cout << setprecision(4);
		int bytes_not_matched = 0;
		for (auto n : unknown_lengths_) bytes_not_matched += n;
		double percentage = bytes_not_matched / mdat->contentSize();
		logg(W, "Unknown sequences: ", unknown_lengths_.size(), '\n');
		logg(W, "Bytes not matched: ", pretty_bytes(bytes_not_matched), " (", percentage, "%)\n");
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

	for(unsigned int t = 0; t < tracks_.size(); t++) {
		Track &track = tracks_[t];
		uint len_chunk_offs = broken_is_64_? track.offsets64_.size() : track.offsets_.size();
		for(uint i = 0; i < len_chunk_offs; i++)
			if(broken_is_64_) track.offsets64_[i] += offset;
			else track.offsets_[i] += offset;

		track.writeToAtoms();  //need to save the offsets back to the atoms
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

void Mp4::analyze(const string& filename) {
	FileRead file;
	bool success = file.open(filename);
	if(!success) throw string("Could not open file: ") + filename;

//	Atom *mdat = root_atom_->atomByName("mdat");
	BufferedAtom* mdat = findMdat(file);
	for(unsigned int i = 0; i < tracks_.size(); i++) {
		Track &track = tracks_[i];
		cout << "\nTrack " << i << " codec: " << track.codec_.name_ << endl;

		const bool is64 = root_atom_->atomByName("co64");
		size_t len_offs = is64? track.offsets64_.size() : track.offsets_.size();
		uint k = track.keyframes_.size() ? track.keyframes_[0] : -1, ik = 0;
		for(unsigned int i = 0; i < len_offs; i++) {
			off64_t offset;
			if (is64) offset = track.offsets64_[i] - (mdat->start_ + 8);
			else offset = track.offsets_[i] - (mdat->start_ + 8);

			uint maxlength = min((int64_t) g_max_partsize, mdat->contentSize() - offset); // maximal 1.6MB
			const uchar *start = mdat->getFragment(offset, maxlength);

			cout << "\n(" << i << ") Size: " << track.sizes_[i] << " offset " << offset
			     << "  begin: " << mkHexStr(start, 4) << " " << mkHexStr(start+4, 4)
			     << " end: " << mkHexStr(start+track.sizes_[i]-4, 4) << '\n';

//			int begin =  mdat->readInt(offset);
//			int next =  mdat->readInt(offset + 4);
//			int end = mdat->readInt(offset + track.sizes_[i] - 4);
//			cout << "(" << i << ") Size: " << track.sizes_[i] << " offset " << track.offsets_[i]
//			        << "  begin: " << hex << begin << " " << next << " end: " << end << dec << endl;

			bool matches = false;
			for (const auto& t : tracks_)
				if (t.codec_.matchSample(start)){
					if (t.codec_.name_ != track.codec_.name_){
						cout << "Matched wrong codec! " << t.codec_.name_ << " instead of " << track.codec_.name_;
						hitEnterToContinue();
					} else {matches = true; break;}
				}
			int duration = 0;
			bool is_bad = 0;
			int size = track.codec_.getSize(start, maxlength, duration, is_bad);
			//TODO check if duration is working with the stts duration.

			if(!matches) {
				cout << "Match failed! " << track.codec_.name_ << " not detected";
				hitEnterToContinue();
			}
			cout << "detected size: " << size << " true: " << track.sizes_[i] << '\n';
			if (track.codec_.name_ == "mp4a") {
				 cout << "detected duration: " << duration << " true: " << track.times_[i] << '\n';
				 if (is_bad) cout << "detected as bad\n";
			}
			if (size != track.sizes_[i] || (track.codec_.name_ == "mp4a" && duration != track.times_[i])) {
				cout << "detected size or duration are wrong!";
				hitEnterToContinue();
			}

			if (k == i){
				cout << "detected keyframe: " << track.codec_.was_keyframe  << " true: 1\n";
				if (!track.codec_.was_keyframe){
					cout << "keyframe not detected!";
					hitEnterToContinue();
				}
				if (++ik < track.keyframes_.size())
					k = track.keyframes_[ik];
			}
			//assert(length == track.sizes[i]);
		}
	}

}

void Mp4::parseTracks() {
	Atom *mdat = root_atom_->atomByName("mdat");
	vector<Atom *> traks = root_atom_->atomsByName("trak");
	for(unsigned int i = 0; i < traks.size(); i++) {
		Track track(traks[i], context_->streams[i]->codecpar);
		track.parse(mdat);
		tracks_.push_back(track);
	}
}

BufferedAtom* Mp4::findMdat(FileRead& file_read) {
	BufferedAtom *mdat = new BufferedAtom(file_read);
	while(1) {

		Atom *atom = new Atom;
		try {
			atom->parseHeader(file_read);
		} catch(string) {
			throw string("Failed to parse atoms in truncated file");
		}

		if(atom->name_ != string("mdat")) {
			int pos = file_read.pos();
			file_read.seek(pos - 8 + atom->length_);
			delete atom;
			continue;
		}

		mdat->start_ = atom->start_;
		memcpy(mdat->name_, atom->name_, 4);
		memcpy(mdat->head_, atom->head_, 4);
		memcpy(mdat->version_, atom->version_, 4);

		//        cout << "file.pos() = " << file_read.pos() << '\n';
		//        cout << "file.length() = " << file_read.length() << '\n';
		mdat->file_begin_ = file_read.pos();
		mdat->file_end_ = file_read.length();
		break;
	}
	return mdat;
}

void Mp4::repair(string& filename, const string& filename_fixed) {
	FileRead file_read;
	if(!file_read.open(filename))
		throw "Could not open file: " + filename;

	//find mdat. fails with krois and a few other.
	//TODO check for multiple mdat, or just look for the first one.

	logg(I, "parsing mdat from truncated file ... \n");
	BufferedAtom* mdat = findMdat(file_read);

	if (file_read.length() > (1LL<<32)) {
		broken_is_64_ = true;
		logg(I, "using 64-bit offsets for the broken file\n");
	}
	for(unsigned int i = 0; i < tracks_.size(); i++)
		tracks_[i].clear();

	//mp4a can be decoded and repors the number of samples (duration in samplerate scale).
	//in some videos the duration (stts) can be variable and we can rebuild them using these values.
	vector<int> audiotimes;
	vector<int64_t> audiotimes64;
	unsigned long cnt_packets = 0;
	off64_t offset = 0;

	logg(V, "mdat->contentSize = ", mdat->contentSize(), '\n');
	int loop_cnt = 0;
	while (offset < mdat->contentSize()) {
		logg(V, "\n(reading element from mdat)\n");
		if (g_log_mode == I &&  loop_cnt++ >= 2000) { // output progress
			outProgress(offset, mdat->file_end_);
			loop_cnt = 0;
		}

		uint maxlength = min((int64_t) g_max_partsize, mdat->contentSize() - offset); // maximal 1.6MB
		const uchar *start = mdat->getFragment(offset, maxlength);

		uint begin = *(uint*)start;

		if(begin == 0) {
			offset += 4;
			continue;
		}

		//Skip zeros to next 000 AARRGH this sometimes is not very correct, unless it's all zeros.
		/*        if(begin == 0) {
			offset &= 0xfffff000;
			offset += 0x1000;
			continue;
		} */
		if (g_log_mode >= LogMode::V) {
			uint begin = swap32(*(uint*)start);
			uint next = swap32(*(uint*)start+4);
//			printBuffer(start, 8);
//			logg(V, "Offset: ", offset, ": ", hex, begin, " ", next, dec, '\n');
			logg(V, "Offset: ", offset, ": ", setfill('0'), setw(8), hex, begin, " ", setw(8), next, dec, '\n');
//			cout << setfill('0') << setw(8) << hex << begin << " "
//			            << setw(8) << next << dec << '\n';
		}


		//skip fake moov
		if(start[4] == 'm' && start[5] == 'o' && start[6] == 'o' && start[7] == 'v') {
			logg(V, "Skipping moov atom: ", swap32(begin), '\n');
			offset += swap32(begin);
			continue;
		}

		bool found = false;
//		uint max_length = 1;
		uint max_length = 4;  // 4 seems to work better
		for(unsigned int i = 0; i < tracks_.size(); i++) {
			Track &track = tracks_[i];
			logg(V, "Track codec: ", track.codec_.name_, '\n');
			//sometime audio packets are difficult to match, but if they are the only ones....
			if (tracks_.size() > 1 && !track.codec_.matchSample(start))
				continue;

			int duration = 0;
			bool is_bad = 0;
			int length_signed = track.codec_.getSize(start, maxlength, duration, is_bad);
			logg(V, "part-length: ", length_signed, '\n');
			if(length_signed < 1) {
				logg(V, "Invalid length: part-length is ", length_signed, '\n');
				continue;
			}

			uint length = static_cast<uint>(length_signed);
			max_length = max(length, max_length);
			if(length > g_max_partsize) {
				logg(E, "Invalid length: ", length, ". Wrong match in track: ", i, '\n');
				continue;
			}
			if(length > maxlength){
				logg(V, "Invalid length: part-length > maxlength\n");
				continue;
			}

			if(track.codec_.name_ == "mp4a" && is_bad){
				logg(V, "skipping mp4a (", length, ") - it is bad\n");
				continue;
			}

			if(unknown_length_){
				logg(V, "found healthy packet again (", track.codec_.name_, "), length: ", length,
				    "duration: ", duration, '\n');
			}

//			if(length <= 8) {
//				logg(V, "Invalid length: ", length, " <= 8");
//				continue;
//			}

			logg(V, "- found as ", track.codec_.name_, '\n');

			bool keyframe = track.codec_.was_keyframe;
			if(keyframe) {
				if(broken_is_64_) track.keyframes_.push_back(track.offsets64_.size());
				else track.keyframes_.push_back(track.offsets_.size()); }
			if(broken_is_64_) track.offsets64_.push_back(offset);
			else track.offsets_.push_back(offset);
			track.sizes_.push_back(length);
			offset += length;
			if(duration)
				audiotimes.push_back(duration);

			found = true;
			track.n_matched++;
			break;
		}

		if(!found && offset < mdat->contentSize()) {
			if (g_ignore_unknown) {
				if (!g_muted && g_log_mode < LogMode::V) {
					logg(I, "unknown sequence -> muting ffmpeg and warnings ..\n");
					mute();
				}
				unknown_length_ += max_length;
				offset += max_length;
				continue;
			}

			if (g_muted) unmute();
			//this could be a problem for large files
			//            assert(mdat->content_.size() + 8 == mdat->length_);
			mdat->file_end_ = mdat->file_begin_ + offset;
			mdat->length_ = mdat->file_end_ - mdat->file_begin_;

			logg(E, "unable to find correct codec -> premature end\n",
			"       try '-s' to skip unknown sequences\n\n");
			logg(V, "mdat->file_end: ", mdat->file_end_, '\n');
			break;
		}
		else if(unknown_length_){
			unknown_lengths_.emplace_back(unknown_length_);
			unknown_length_ = 0;
//			unmute();
		}

		cnt_packets++;
	}

	if (g_muted) unmute();
	if(unknown_length_) {
		unknown_lengths_.emplace_back(unknown_length_);
		unknown_length_ = 0;
	}

	if(g_log_mode >= I){
		cout << "Info: Found " << cnt_packets << " packets ( ";
		for(const Track& t : tracks_){
			cout << t.codec_.name_ << ": " << t.n_matched << ' ';
			if (t.codec_.name_ == "avc1")
				cout << "avc1-keyframes: " << t.keyframes_.size() << " ";
		}
		cout << ")\n";
	}

	for(auto& track : tracks_) {
		if(audiotimes.size() == track.offsets_.size() || audiotimes.size() == track.offsets64_.size())
			swap(audiotimes, track.times_);
		track.fixTimes(broken_is_64_);
		int track_duration = (int)(double)track.duration_ * ((double)timescale_ / (double)track.timescale_);
		if(track_duration > duration_) duration_ = track_duration;
		track.duration_in_timescale_ = track_duration;
	}
	Atom *original_mdat = root_atom_->atomByName("mdat");
	mdat->start_ = original_mdat->start_;
	root_atom_->replace(original_mdat, mdat);

	saveVideo(filename_fixed);
}
