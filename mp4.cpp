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

#define __STDC_LIMIT_MACROS 1
#define __STDC_CONSTANT_MACROS 1
extern "C" {
#include <stdint.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

#include "mp4.h"
#include "atom.h"
#include "file.h"

using namespace std;

Mp4::Mp4(): root(NULL) { }

Mp4::~Mp4() {

	delete root;
}

void Mp4::open(string filename) {
	File file;
	bool success = file.open(filename);
	if(!success) throw string("Could not open file: ") + filename;

	root = new Atom;
	while(1) {
		Atom *atom = new Atom;
		atom->parse(file);
		root->children.push_back(atom);
		if(file.atEnd()) break;
	}

	if(root->atomByName("ctts"))
		cerr << "Composition time offset atom found. Out of order samples possible." << endl;

	if(root->atomByName("sdtp"))
		cerr << "Sample dependency flag atom found. I and P frames might need to recover that info." << endl;

	Atom *mvhd = root->atomByName("mvhd");
	if(!mvhd)
		throw string("Missing movie header atom");
	timescale = mvhd->readInt(12);
	duration = mvhd->readInt(16);

	av_register_all();
	context = avformat_alloc_context();
	// Open video file
	//int error = av_open_input_file(&context, filename.c_str(), NULL, 0, NULL);
	int error = avformat_open_input(&context, filename.c_str(), NULL, NULL);

	if(error != 0)
		throw "Could not parse AV file: " + filename;

	//if(av_find_stream_info(context)<0)
	if(avformat_find_stream_info(context, NULL) < 0)
		throw string("Could not find stream info");

	av_dump_format(context, 0, filename.c_str(), 0);

	parseTracks();
}

void Mp4::printMediaInfo() {

}

void Mp4::printAtoms() {
	if(root)
		root->print(0);
}

void Mp4::makeStreamable(string filename, string output) {
	Atom *root = new Atom;
	{
		File file;
		bool success = file.open(filename);
		if(!success) throw string("Could not open file: ") + filename;

		while(1) {
			Atom *atom = new Atom;
			atom->parse(file);
			cout << "Found atom: " << atom->name << endl;
			root->children.push_back(atom);
			if(file.atEnd()) break;
		}
	}
	Atom *ftyp = root->atomByName("ftyp");
	Atom *moov = root->atomByName("moov");
	Atom *mdat = root->atomByName("mdat");

	if(mdat->start > moov->start) {
		cout << "FIle is already streamable" << endl;
		return;
	}
	int old_start = (mdat->start + 8);

	int new_start = moov->length+8;
	if(ftyp)
		new_start += ftyp->length + 8;

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
	for(int i = 0; i < stcos.size(); i++) {
		Atom *stco = stcos[i];
		int offsets = stco->readInt(4);   //4 version, 4 number of entries, 4 entries
		for(unsigned int i = 0; i < offsets; i++) {
			int pos = 8 + 4*i;
			int offset = stco->readInt(pos) + diff;
			cout << "O: " << offset << endl;
			stco->writeInt(offset, pos);
		}
	}

	{
		//save to file
		File file;
		if(!file.create(output))
			throw "Could not create file for writing: " + output;

		if(ftyp)
			ftyp->write(file);
		moov->write(file);
		mdat->write(file);
	}
}

void Mp4::saveVideo(string filename) {
	/* we save all atom except:
	  ctts: composition offset ( we use sample to time)
	  cslg: because it is used only when ctts is present
	  stps: partial sync, same as sync

	  movie is made by ftyp, moov, mdat (we need to know mdat begin, for absolute offsets)
	  assumes offsets in stco are absolute and so to find the relative just subtrack mdat->start + 8
*/

	duration = 0;
	for(unsigned int i = 0; i < tracks.size(); i++) {
		Track &track = tracks[i];
		track.writeToAtoms();
		//convert to movie timescale
		cout << "Track duration: " << track.duration << " movie timescale: " << timescale << " track timescale: " << track.timescale << endl;
		int track_duration = (int)(double)track.duration * ((double)timescale / (double)track.timescale);
		if(track_duration > duration) duration = track_duration;

		Atom *tkhd = track.trak->atomByName("tkhd");
		tkhd->writeInt(track_duration, 20); //in movie timescale, not track timescale
	}
	Atom *mvhd = root->atomByName("mvhd");
	mvhd->writeInt(duration, 16);


	Atom *ftyp = root->atomByName("ftyp");
	Atom *moov = root->atomByName("moov");
	Atom *mdat = root->atomByName("mdat");

	moov->prune("ctts");
	moov->prune("cslg");
	moov->prune("stps");

	root->updateLength();

	//fix offsets
	int offset = 8 + moov->length;
	if(ftyp)
		offset += ftyp->length; //not all mov have a ftyp.

	for(unsigned int t = 0; t < tracks.size(); t++) {
		Track &track = tracks[t];
		for(unsigned int i = 0; i < track.offsets.size(); i++)
			track.offsets[i] += offset;

		track.writeToAtoms();  //need to save the offsets back to the atoms
	}

	//save to file
	File file;
	if(!file.create(filename))
		throw "Could not create file for writing: " + filename;

	if(ftyp)
		ftyp->write(file);
	moov->write(file);
	mdat->write(file);
}

void Mp4::analyze() {


	Atom *mdat = root->atomByName("mdat");
	for(unsigned int i = 0; i < tracks.size(); i++) {
		cout << "\n\n Track " << i << endl;
		Track &track = tracks[i];

		cout << "Track codec: " << track.codec.name << endl;

		cout << "Keyframes  " << track.keyframes.size() << endl;
		for(unsigned int i = 0; i < track.keyframes.size(); i++) {
			int k = track.keyframes[i];
			int offset = track.offsets[k] - (mdat->start + 8);
			int begin =  mdat->readInt(offset);
			int next =  mdat->readInt(offset + 4);
			cout << "\n" << k << " Size: " << track.sizes[k] << " offset " << track.offsets[k]
					<< "  begin: " << hex << begin << " " << next << dec << endl;
		}

		for(unsigned int i = 0; i < track.offsets.size(); i++) {
			int offset = track.offsets[i] - (mdat->start + 8);
			unsigned char *start = &(mdat->content[offset]);
			int maxlength = mdat->content.size() - offset;

			int begin =  mdat->readInt(offset);
			int next =  mdat->readInt(offset + 4);
			int end = mdat->readInt(offset + track.sizes[i] - 4);
			cout << i << " Size: " << track.sizes[i] << " offset " << track.offsets[i]
					<< "  begin: " << hex << begin << " " << next << " end: " << end << dec << endl;

			bool matches = track.codec.matchSample(start, maxlength);
			int duration = 0;
			int length= track.codec.getLength(start, maxlength, duration);
			//TODO check if duration is working with the stts duration.

			if(!matches) {
				cout << "Match failed! Hit enter for next match." << endl;
				getchar();
			}
			//assert(matches);
			cout << "Length: " << length << " true length: " << track.sizes[i] << endl;
			if(length != track.sizes[i])
				getchar();

			//assert(length == track.sizes[i]);

		}
	}

}

void Mp4::writeTracksToAtoms() {
	for(unsigned int i = 0; i < tracks.size(); i++)
		tracks[i].writeToAtoms();
}

void Mp4::parseTracks() {
	Atom *mdat = root->atomByName("mdat");
	vector<Atom *> traks = root->atomsByName("trak");
	for(unsigned int i = 0; i < traks.size(); i++) {
		Track track;
		track.codec.context = context->streams[i]->codec;
		track.parse(traks[i], mdat);
		tracks.push_back(track);
	}
}

void Mp4::repair(string filename) {


	File file;
	if(!file.open(filename))
		throw "Could not open file: " + filename;

	//find mdat. fails with krois and a few other.
	//TODO check for multiple mdat, or just look for the first one.
	BufferedAtom *mdat = new BufferedAtom(filename);
	while(1) {

		Atom *atom = new Atom;
		try {
			atom->parseHeader(file);
		} catch(string) {
			throw string("Failed to parse atoms in truncated file");
		}

		if(atom->name != string("mdat")) {
			int pos = file.pos();
			file.seek(pos - 8 + atom->length);
			delete atom;
			continue;
		}

		mdat->start = atom->start;
		memcpy(mdat->name, atom->name, 4);
		memcpy(mdat->head, atom->head, 4);
		memcpy(mdat->version, atom->version, 4);

		mdat->file_begin = file.pos();
		mdat->file_end = file.length() - file.pos();
		//mdat->content = file.read(file.length() - file.pos());
		break;
	}


	for(unsigned int i = 0; i < tracks.size(); i++)
		tracks[i].clear();

	//mp4a is more reliable than avc1.
	if(tracks.size() > 1 && tracks[1].codec.name == "mp4a") {
		Track tmp = tracks[0];
		tracks[0] = tracks[1];
		tracks[1] = tmp;
	}

	//mp4a can be decoded and repors the number of samples (duration in samplerate scale).
	//in some videos the duration (stts) can be variable and we can rebuild them using these values.
	vector<int> audiotimes;
	unsigned long count = 0;
	off_t offset = 0;
	while(offset < mdat->contentSize()) {

		//unsigned char *start = &(mdat->content[offset]);
		int64_t maxlength = mdat->contentSize() - offset;
		if(maxlength > 1600000) maxlength = 1600000;
		unsigned char *start = mdat->getFragment(offset, maxlength);

		uint begin =  mdat->readInt(offset);


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
		uint next =  mdat->readInt(offset + 4);

#ifdef VERBOSE1
		cout << "Offset: " << offset << " ";
		cout << hex << begin << " " << next << dec << endl;
#endif

		//skip fake moov
		if(start[4] == 'm' && start[5] == 'o' && start[6] == 'o' && start[7] == 'v') {
			cout << "Skipping moov atom: " << swap32(begin) << endl;
			offset += swap32(begin);
			continue;
		}

		bool found = false;
		for(unsigned int i = 0; i < tracks.size(); i++) {
			Track &track = tracks[i];
			cout << "Track codec: " << track.codec.name << endl;
			//sometime audio packets are difficult to match, but if they are the only ones....
			int duration =0;
			if(tracks.size() > 1 && !track.codec.matchSample(start, maxlength)) continue;
			int length = track.codec.getLength(start, maxlength, duration);
			if(length < -1 || length > 1600000) {
				cout << endl << "Invalid length. " << length << ". Wrong match in track: " << i << endl;
				continue;
			}
			if(length == -1 || length == 0) {
				continue;
			}
			if(length >= maxlength)
				continue;
#ifdef VERBOSE1
			if(length > 8)
				cout << ": found as " << track.codec.name << endl;
#endif
			bool keyframe = track.codec.isKeyframe(start, maxlength);
			if(keyframe)
				track.keyframes.push_back(track.offsets.size());
			track.offsets.push_back(offset);
			track.sizes.push_back(length);
			offset += length;

			if(duration)
				audiotimes.push_back(duration);

			found = true;
			break;
		}

#ifdef VERBOSE1
		cout << endl;
#endif

		if(!found) {
			//this could be a problem for large files
			//assert(mdat->content.size() + 8 == mdat->length);
			mdat->file_end = mdat->file_begin + offset;
			mdat->length = mdat->file_end - mdat->file_begin;
			//mdat->content.resize(offset);
			//mdat->length = mdat->content.size() + 8;
			break;
		}
		count++;
	}

	cout << "Found " << count << " packets\n";

	for(unsigned int i = 0; i < tracks.size(); i++) {
		if(audiotimes.size() == tracks[i].offsets.size())
			swap(audiotimes, tracks[i].times);

		tracks[i].fixTimes();
	}

	Atom *original_mdat = root->atomByName("mdat");
	mdat->start = original_mdat->start;
	root->replace(original_mdat, mdat);
	//original_mdat->content.swap(mdat->content);
	//original_mdat->start = -8;
}
