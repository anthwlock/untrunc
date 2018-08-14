//==================================================================//
/*                                                                  *
	Untrunc - track.cpp

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

#include "track.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}
#include "atom.h"


using std::cout;
using std::endl;
using std::pair;
using std::string;
using std::vector;


Track::Track(Atom* t, AVCodecContext* c) : trak_(t), codec_(c), n_matched(0) { }

void Track::parse(Atom* mdat) {
	Atom* mdhd = trak_->atomByName("mdhd");
	if (!mdhd)
		throw string("No mdhd atom: unknown duration and timescale");

	timescale_ = mdhd->readInt(12);
	duration_ = mdhd->readInt(16);

	times_ = getSampleTimes(trak_);
	keyframes_ = getKeyframes(trak_);
	sizes_ = getSampleSizes(trak_);

	vector<int> chunk_offsets = getChunkOffsets(trak_);
	vector<int> sample_to_chunk = getSampleToChunk(trak_, chunk_offsets.size());

	if (times_.size() != sizes_.size()) {
		cout << "Mismatch between time offsets and size offsets: \n";
		cout << "Time offsets: " << times_.size()
			 << " Size offsets: " << sizes_.size() << endl;
	}
	//assert(times.size() == sizes.size());
	if (times_.size() != sample_to_chunk.size()) {
		cout << "Mismatch between time offsets and sample_to_chunk offsets: \n";
		cout << "Time offsets: " << times_.size()
			 << " Chunk offsets: " << sample_to_chunk.size() << endl;
	}
	// Compute actual offsets.
	int old_chunk = -1;
	int offset = -1;
	for (unsigned int i = 0; i < sizes_.size(); i++) {
		int chunk = sample_to_chunk[i];
		int size = sizes_[i];
		if (chunk != old_chunk) {
			offset = chunk_offsets[chunk];
			old_chunk = chunk;
		}
		offsets_.push_back(offset);
		offset += size;
	}

	// Move this stuff into track!
	Atom* hdlr = trak_->atomByName("hdlr");
	char type[5];
	hdlr->readChar(type, 8, 4);

	//bool audio = (type == string("soun"));

	if (type != string("soun") && type != string("vide"))
		return;

	// Move this to Codec.

	codec_.parse(trak_, offsets_, mdat);
	// If audio, use next?

#if 0
	if (!codec.codec)
		throw string("No codec found!");
	if (avcodec_open2(codec.context, codec.codec, NULL) < 0)
		throw string("Could not open codec: ") + codec.context->codec_name;
#endif

#if 0
	Atom *mdat = root->atomByName("mdat");
	if(!mdat)
		throw string("Missing data atom");

	// Print sizes and offsets.
	for (int i = 0; i < 10 && i < sizes.size(); i++) {
		cout << "offset: " << offsets[i] << " size: " << sizes[i] << endl;
		cout << mdat->readInt(offsets[i] - (mdat->start + 8)) << endl;
	}
#endif
}

void Track::writeToAtoms() {
	if (!keyframes_.size())
		trak_->prune("stss");

	saveSampleTimes();
	saveKeyframes();
	saveSampleSizes();
	saveSampleToChunk();
	saveChunkOffsets();

	Atom* mdhd = trak_->atomByName("mdhd");
	mdhd->writeInt(duration_, 16);

	// Avc1 codec writes something inside stsd.
	// In particular the picture parameter set (PPS) in avcC
	//  (after the sequence parameter set).
	// For avcC see: <http://jaadec.sourceforge.net/specs/ISO_14496-15_AVCFF.pdf>.
	// For PPS  see: <http://www.iitk.ac.in/mwn/vaibhav/Vaibhav%20Sharma_files/h.264_standard_document.pdf>
	// I have found out in shane, banana, bruno and nawfel that
	//  pic_init_qp_minus26 is different even for consecutive videos
	//  of the same camera.
	// The only thing to do then is to test possible values,
	//  to do so remove 28 (the NAL header) follow the golomb stuff.
	// This test could be done automatically when decoding fails...
	//#define SHANE 6
#ifdef SHANE
	int pps[15] = {0x28ee3880, 0x28ee1620, 0x28ee1e20, 0x28ee0988, 0x28ee0b88,
				   0x28ee0d88, 0x28ee0f88, 0x28ee0462, 0x28ee04e2, 0x28ee0562,
				   0x28ee05e2, 0x28ee0662, 0x28ee06e2, 0x28ee0762, 0x28ee07e2};
	if (codec.name == "avc1") {
		Atom* stsd = trak->atomByName("stsd");
		stsd->writeInt(pps[SHANE],
					   122);  // A bit complicated to find... find avcC... follow first link.
	}
#endif
}

void Track::clear() {
	//times_.clear();
	offsets_.clear();
	sizes_.clear();
	keyframes_.clear();
}

void Track::fixTimes() {
	if (codec_.name_ == "samr") {
		times_.clear();
		times_.resize(offsets_.size(), 160);
		return;
	}
	//for(int i = 0; i != 100; i++)
	//	cout << "times_[" << i << "] = " << times_[i] << '\n';
	while (times_.size() < offsets_.size())
		times_.insert(times_.end(), times_.begin(), times_.end());
	times_.resize(offsets_.size());

	duration_ = 0;
	for (unsigned int i = 0; i < times_.size(); i++)
		duration_ += times_[i];
}

vector<int> Track::getSampleTimes(Atom* t) {
	vector<int> sample_times;
	// Chunk offsets.
	Atom* stts = t->atomByName("stts");
	if (!stts)
		throw string("Missing sample to time atom");

	int entries = stts->readInt(4);
	for (int i = 0; i < entries; i++) {
		int nsamples = stts->readInt(8 + 8 * i);
		int time = stts->readInt(12 + 8 * i);
		for (int i = 0; i < nsamples; i++)
			sample_times.push_back(time);
	}
	return sample_times;
}

vector<int> Track::getKeyframes(Atom* t) {
	vector<int> sample_key;
	// Chunk offsets.
	Atom* stss = t->atomByName("stss");
	if (!stss)
		return sample_key;

	int entries = stss->readInt(4);
	for (int i = 0; i < entries; i++)
		sample_key.push_back(stss->readInt(8 + 4 * i) - 1);
	return sample_key;
}

vector<int> Track::getSampleSizes(Atom* t) {
	vector<int> sample_sizes;
	// Chunk offsets.
	Atom* stsz = t->atomByName("stsz");
	if (!stsz)
		throw string("Missing sample to sizeatom");

	int entries = stsz->readInt(8);
	int default_size = stsz->readInt(4);
	if (default_size == 0) {
		for (int i = 0; i < entries; i++)
			sample_sizes.push_back(stsz->readInt(12 + 4 * i));
	} else {
		sample_sizes.resize(entries, default_size);
	}
	return sample_sizes;
}


vector<int> Track::getChunkOffsets(Atom* t) {
	vector<int> chunk_offsets;
	// Chunk offsets.
	Atom* stco = t->atomByName("stco");
	if (stco) {
		int nchunks = stco->readInt(4);
		for (int i = 0; i < nchunks; i++)
			chunk_offsets.push_back(stco->readInt(8 + i * 4));

	} else {
		Atom* co64 = t->atomByName("co64");
		if (!co64)
			throw string("Missing chunk offset atom");

		int nchunks = co64->readInt(4);
		for (int i = 0; i < nchunks; i++)
			chunk_offsets.push_back(co64->readInt(12 + i * 8));
	}
	return chunk_offsets;
}

vector<int> Track::getSampleToChunk(Atom* t, int nchunks) {
	vector<int> sample_to_chunk;

	Atom* stsc = t->atomByName("stsc");
	if (!stsc)
		throw string("Missing sample to chunk atom");

	vector<int> first_chunks;
	int entries = stsc->readInt(4);
	for (int i = 0; i < entries; i++)
		first_chunks.push_back(stsc->readInt(8 + 12 * i));
	first_chunks.push_back(nchunks + 1);

	for (int i = 0; i < entries; i++) {
		int first_chunk = first_chunks[i];
		int last_chunk = first_chunks[i + 1];
		int n_samples = stsc->readInt(12 + 12 * i);

		for (int k = first_chunk; k < last_chunk; k++) {
			for (int j = 0; j < n_samples; j++)
				sample_to_chunk.push_back(k - 1);
		}
	}

	return sample_to_chunk;
}


void Track::saveSampleTimes() {
	Atom* stts = trak_->atomByName("stts");
	assert(stts);
	vector<pair<int, int>> vp;
	for (uint i = 0; i < times_.size(); i++) {
		if (vp.empty() || times_[i] != vp.back().second)
			vp.emplace_back(1, times_[i]);
		else
			vp.back().first++;
	}
	stts->content_.resize(4 +              // Version.
						  4 +              // Number of entries.
						  8 * vp.size());  // Time table.
	stts->writeInt(vp.size(), 4);
	int cnt = 0;
	for (auto p : vp) {
		stts->writeInt(p.first, 8 + 8 * cnt);    // sample_count.
		stts->writeInt(p.second, 12 + 8 * cnt);  // sample_time_delta.
		cnt++;
	}
}

void Track::saveKeyframes() {
	Atom* stss = trak_->atomByName("stss");
	if (!stss)
		return;
	assert(keyframes_.size());
	if (!keyframes_.size())
		return;

	stss->content_.resize(4 +                      // Version.
						  4 +                      // Number of entries.
						  4 * keyframes_.size());  // Time table.
	stss->writeInt(keyframes_.size(), 4);
	for (unsigned int i = 0; i < keyframes_.size(); i++)
		stss->writeInt(keyframes_[i] + 1, 8 + 4 * i);
}

void Track::saveSampleSizes() {
	Atom* stsz = trak_->atomByName("stsz");
	assert(stsz);
	stsz->content_.resize(4 +                  // Version.
						  4 +                  // Default size.
						  4 +                  // Number of entries.
						  4 * sizes_.size());  // Size table.
	stsz->writeInt(0, 4);
	stsz->writeInt(sizes_.size(), 8);
	for (unsigned int i = 0; i < sizes_.size(); i++) {
		stsz->writeInt(sizes_[i], 12 + 4 * i);
	}
}

void Track::saveSampleToChunk() {
	Atom* stsc = trak_->atomByName("stsc");
	assert(stsc);
	stsc->content_.resize(4 +   // Version.
						  4 +   // Number of entries.
						  12);  // One sample per chunk.
	stsc->writeInt(1, 4);
	stsc->writeInt(1, 8);   // First chunk (1 based).
	stsc->writeInt(1, 12);  // One sample per chunk.
	stsc->writeInt(1, 16);  // Id 1. (XXX WHAT IS THIS! XXX)
}

void Track::saveChunkOffsets() {
	Atom* co64 = trak_->atomByName("co64");
	if (co64) {
		trak_->prune("co64");
		Atom* stbl = trak_->atomByName("stbl");
		Atom* new_stco = new Atom;
		memcpy(new_stco->name_, "stco", 5);
		stbl->children_.push_back(new_stco);
	}
	Atom* stco = trak_->atomByName("stco");
	assert(stco);
	stco->content_.resize(4 +  // Version.
						  4 +  // Number of entries.
						  4 * offsets_.size());
	stco->writeInt(offsets_.size(), 4);
	for (unsigned int i = 0; i < offsets_.size(); i++)
		stco->writeInt(offsets_[i], 8 + 4 * i);
}


// vim:set ts=4 sw=4 sts=4 noet:
