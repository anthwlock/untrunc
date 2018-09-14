/*
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

														*/

#include "track.h"

#include <iostream>
#include <vector>
#include <string.h>
#include <assert.h>


extern "C" {
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "atom.h"

using namespace std;

Track::Track(Atom *t, AVCodecContext *c) : trak_(t), codec_(c), n_matched(0) {

}

/* parse healthy trak atom */
void Track::parse(Atom *mdat) {

	Atom *mdhd = trak_->atomByName("mdhd");
	if(!mdhd)
		throw string("No mdhd atom: unknown duration and timescale");

	timescale_ = mdhd->readInt(12);
	duration_ = mdhd->readInt(16);
	const bool is64 = trak_->atomByName("co64");

	times_ = getSampleTimes(trak_);
	keyframes_ = getKeyframes(trak_);
	sizes_ = getSampleSizes(trak_);

	vector<int64_t> chunk_offsets_64;
	vector<uint> chunk_offsets;
	if(is64) chunk_offsets_64 = getChunkOffsets64(trak_);
	else chunk_offsets = getChunkOffsets(trak_);
	int len_chunk_offs = is64? chunk_offsets_64.size() : chunk_offsets.size();
	vector<int> sample_to_chunk = getSampleToChunk(trak_, len_chunk_offs);

	if(times_.size() != sizes_.size()) {
		cout << "Mismatch between time offsets and size offsets: \n";
		cout << "Time offsets: " << times_.size() << " Size offsets: " << sizes_.size() << endl;
	}
	//assert(times.size() == sizes.size());
	if(times_.size() != sample_to_chunk.size()) {
		cout << "Mismatch between time offsets and sample_to_chunk offsets: \n";
		cout << "Time offsets: " << times_.size() << " Chunk offsets: " << sample_to_chunk.size() << endl;
	}
	//compute actual offsets
	int64_t old_chunk = -1;
	int64_t offset = -1;
	for(unsigned int i = 0; i < sizes_.size(); i++) {
		int chunk = sample_to_chunk[i];
		int size = sizes_[i];
		if(chunk != old_chunk) {
			if(is64) offset = chunk_offsets_64[chunk];
			else offset = chunk_offsets[chunk];
//			cout << "- offset: " << offset << '\n';
			old_chunk= chunk;
		}
		if(is64) offsets64_.push_back(offset);
		else offsets_.push_back(offset);
		offset += size;
	}
	//move this stuff into track!
	Atom *hdlr = trak_->atomByName("hdlr");
	char type[5];
	hdlr->readChar(type, 8, 4);

	//	bool audio = (type == string("soun"));

	if(type != string("soun") && type != string("vide"))
		return;

	//	//move this to Codec

	codec_.parse(trak_, offsets_, offsets64_, mdat, is64);
	//if audio use next?

//	if(!codec.codec) throw string("No codec found!");
//	if(avcodec_open2(codec.context, codec.codec, NULL)<0)
//		throw string("Could not open codec: ") + codec.context->codec_name;


	/*
	Atom *mdat = root->atomByName("mdat");
	if(!mdat)
		throw string("Missing data atom");

	//print sizes and offsets
		for(int i = 0; i < 10 && i < sizes.size(); i++) {
		cout << "offset: " << offsets[i] << " size: " << sizes[i] << endl;
		cout << mdat->readInt(offsets[i] - (mdat->start + 8)) << endl;
	} */
}

void Track::writeToAtoms() {

	if (!keyframes_.size())
		trak_->prune("stss");

	saveSampleTimes();
	saveKeyframes();
	saveSampleSizes();
	saveSampleToChunk();
	saveChunkOffsets();

	Atom *mdhd = trak_->atomByName("mdhd");
	mdhd->writeInt(duration_, 16);

	//Avc1 codec writes something inside stsd.
	//In particular the picture parameter set (PPS) in avcC (after the sequence parameter set)
	//See http://jaadec.sourceforge.net/specs/ISO_14496-15_AVCFF.pdf for the avcC stuff
	//See http://www.iitk.ac.in/mwn/vaibhav/Vaibhav%20Sharma_files/h.264_standard_document.pdf for the PPS
	//I have found out in shane,banana,bruno and nawfel that pic_init_qp_minus26  is different even for consecutive videos of the same camera.
	//the only thing to do then is to test possible values, to do so remove 28 (the nal header) follow the golomb stuff
	//This test could be done automatically when decoding fails..
	//#define SHANE 6
#ifdef SHANE
	int pps[15] = {
			0x28ee3880,
			0x28ee1620,
			0x28ee1e20,
			0x28ee0988,
			0x28ee0b88,
			0x28ee0d88,
			0x28ee0f88,
			0x28ee0462,
			0x28ee04e2,
			0x28ee0562,
			0x28ee05e2,
			0x28ee0662,
			0x28ee06e2,
			0x28ee0762,
			0x28ee07e2
	};
	if(codec.name == "avc1") {
		Atom *stsd = trak->atomByName("stsd");
		stsd->writeInt(pps[SHANE], 122); //a bit complicated to find.... find avcC... follow first link.
	}
#endif

}

void Track::clear() {
//	times_.clear();
	offsets_.clear();
	offsets64_.clear();
	sizes_.clear();
	keyframes_.clear();
}

void Track::fixTimes(const bool is64) {
	const size_t len_offs = is64? offsets64_.size() : offsets_.size();
	if(codec_.name_ == "samr") {
		times_.clear();
		times_.resize(len_offs, 160);
		return;
	}
//	for(int i=0; i != 100; i++)
//		cout << "times_[" << i << "] = " << times_[i] << '\n';
	while(times_.size() < len_offs)
		times_.insert(times_.end(), times_.begin(), times_.end());
	times_.resize(len_offs);

	duration_ = 0;
	for(auto time : times_)
		duration_ += time;
}

vector<int> Track::getSampleTimes(Atom *t) {
	vector<int> sample_times;
	//chunk offsets
	Atom *stts = t->atomByName("stts");
	if(!stts)
		throw string("Missing sample to time atom");

	int entries = stts->readInt(4);
	for(int i = 0; i < entries; i++) {
		int nsamples = stts->readInt(8 + 8*i);
		int time = stts->readInt(12 + 8*i);
		for(int i = 0; i < nsamples; i++)
			sample_times.push_back(time);
	}
	return sample_times;
}

vector<int> Track::getKeyframes(Atom *t) {
	vector<int> sample_key;
	//chunk offsets
	Atom *stss = t->atomByName("stss");
	if(!stss)
		return sample_key;

	int entries = stss->readInt(4);
	for(int i = 0; i < entries; i++)
		sample_key.push_back(stss->readInt(8 + 4*i) - 1);
	return sample_key;
}

vector<int> Track::getSampleSizes(Atom *t) {
	vector<int> sample_sizes;
	//chunk offsets
	Atom *stsz = t->atomByName("stsz");
	if(!stsz)
		throw string("Missing sample to sizeatom");

	int entries = stsz->readInt(8);
	int default_size = stsz->readInt(4);
	if(default_size == 0) {
		for(int i = 0; i < entries; i++)
			sample_sizes.push_back(stsz->readInt(12 + 4*i));
	} else {
		sample_sizes.resize(entries, default_size);
	}
	return sample_sizes;
}



vector<int64_t> Track::getChunkOffsets64(Atom *trak) {
	vector<int64_t> chunk_offsets;
	Atom *co64 = trak->atomByName("co64");
	if(!co64)
		throw string("Missing chunk offset atom 'co64'");

	int nchunks = co64->readInt(4);
	for(int i = 0; i < nchunks; i++) {
		int64_t off = co64->readInt64(8 + i*8);
		chunk_offsets.push_back(off);
	}
	return chunk_offsets;
}

vector<uint> Track::getChunkOffsets(Atom *trak) {
	vector<uint> chunk_offsets;
	//chunk offsets
	Atom *stco = trak->atomByName("stco");
	if(!stco)
		throw string("Missing chunk offset atom 'stco'");
	int nchunks = stco->readInt(4);
	for(int i = 0; i < nchunks; i++)
		chunk_offsets.push_back(stco->readInt(8 + i*4));
	return chunk_offsets;
}

vector<int> Track::getSampleToChunk(Atom *t, int nchunks){
	vector<int> sample_to_chunk;

	Atom *stsc = t->atomByName("stsc");
	if(!stsc)
		throw string("Missing sample to chunk atom");

	vector<int> first_chunks;
	int entries = stsc->readInt(4);
	for(int i = 0; i < entries; i++)
		first_chunks.push_back(stsc->readInt(8 + 12*i));
	first_chunks.push_back(nchunks+1);

	for(int i = 0; i < entries; i++) {
		int first_chunk = first_chunks[i];
		int last_chunk = first_chunks[i+1];
		int n_samples = stsc->readInt(12 + 12*i);

		for(int k = first_chunk; k < last_chunk; k++) {
			for(int j = 0; j < n_samples; j++)
				sample_to_chunk.push_back(k-1);
		}
	}

	return sample_to_chunk;
}


void Track::saveSampleTimes() {
	Atom *stts = trak_->atomByName("stts");
	assert(stts);
	vector<pair<int,int>> vp;
	for (uint i = 0; i < times_.size(); i++){
		if (vp.empty() || times_[i] != vp.back().second)
			vp.emplace_back(1, times_[i]);
		else
			vp.back().first++;
	}
	stts->content_.resize(4 + //version
	                      4 + //entries
	                      8*vp.size()); //time table
	stts->writeInt(vp.size(), 4);
	int cnt = 0;
	for(auto p : vp) {
		stts->writeInt(p.first, 8 + 8*cnt); // sample_count
		stts->writeInt(p.second, 12 + 8*cnt); // sample_time_delta
		cnt++;
	}
}

void Track::saveKeyframes() {
	Atom *stss = trak_->atomByName("stss");
	if(!stss) return;
	assert(keyframes_.size());
	if(!keyframes_.size()) return;

	stss->content_.resize(4 + //version
						  4 + //entries
						  4*keyframes_.size()); //time table
	stss->writeInt(keyframes_.size(), 4);
	for(unsigned int i = 0; i < keyframes_.size(); i++)
		stss->writeInt(keyframes_[i] + 1, 8 + 4*i);
}

void Track::saveSampleSizes() {
	Atom *stsz = trak_->atomByName("stsz");
	assert(stsz);
	stsz->content_.resize(4 + //version
						  4 + //default size
						  4 + //entries
						  4*sizes_.size()); //size table
	stsz->writeInt(0, 4);
	stsz->writeInt(sizes_.size(), 8);
	for(unsigned int i = 0; i < sizes_.size(); i++) {
		stsz->writeInt(sizes_[i], 12 + 4*i);
	}
}

void Track::saveSampleToChunk() {
	Atom *stsc = trak_->atomByName("stsc");
	assert(stsc);
	stsc->content_.resize(4 + // version
						  4 + //number of entries
						  12); //one sample per chunk.
	stsc->writeInt(1, 4);
	stsc->writeInt(1, 8); //first chunk (1 based)
	stsc->writeInt(1, 12); //one sample per chunk
	stsc->writeInt(1, 16); //id 1 (WHAT IS THIS!)
}

void Track::saveChunkOffsets() {
	Atom *co64 = trak_->atomByName("co64");
	if(co64) {
		co64->content_.resize(4 + //version
		                      4 + //number of entries
		                      8*offsets64_.size());
		co64->writeInt(offsets64_.size(), 4);
		for(unsigned int i = 0; i < offsets64_.size(); i++)
			co64->writeInt64(offsets64_[i], 8 + 8*i);
	}
	else {
		Atom *stco = trak_->atomByName("stco");
		assert(stco);
		stco->content_.resize(4 + //version
		                      4 + //number of entries
		                      4*offsets_.size());
		stco->writeInt(offsets_.size(), 4);
		for(unsigned int i = 0; i < offsets_.size(); i++)
			stco->writeInt(offsets_[i], 8 + 4*i);
	}
}


