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

Track::Track(Atom *t, AVCodecParameters *c, int ts) : trak_(t), codec_(c),
    mp4_timescale_(ts), n_matched(0) {}

int Track::getOrigSize(uint idx) {
	if (orig_sizes_.size())
		return orig_sizes_[idx];
	else
		return sizes_[idx];
}

void Track::parseOk() {
	header_atom_ = trak_->atomByName("mdhd");
	if(!header_atom_)
		throw "No mdhd atom: unknown duration and timescale";
	readHeaderAtom();

	times_ = getSampleTimes();
	keyframes_ = getKeyframes();
	sizes_ = getSampleSizes();

	auto chunk_offsets = trak_->atomByName("co64") ? getChunkOffsets64() : getChunkOffsets();
	auto sample_to_chunk = getSampleToChunk(chunk_offsets.size());

	if (times_.size() != sizes_.size()) {
		cout << "Mismatch between time offsets and size offsets: \n";
		cout << "Time offsets: " << times_.size() << " Size offsets: " << sizes_.size() << endl;
	}
	if (times_.size() != sample_to_chunk.size()) {
		cout << "Mismatch between time offsets and sample_to_chunk offsets: \n";
		cout << "Time offsets: " << times_.size() << " Chunk offsets: " << sample_to_chunk.size() << endl;
	}

	//compute actual offsets
	int64_t offset = -1, old_chunk = -1;
	for(uint i = 0; i < sizes_.size(); i++) {
		int chunk = sample_to_chunk[i];
		if (chunk != old_chunk) {
			offset = chunk_offsets[chunk];
			old_chunk = chunk;
		}
		offsets_.push_back(offset);
		offset += sizes_[i];
	}

	Atom *hdlr = trak_->atomByName("hdlr");
	handler_type_ = hdlr->getString(8, 4);

	int name_size = hdlr->length_ - (24+8);
	handler_name_ = hdlr->getString(24, name_size);
	if (handler_name_[0] == name_size-1)  // special case: pascal string
		handler_name_.erase(0, 1);
	trim_right(handler_name_);

	if (handler_type_ != "soun" && handler_type_ != "vide" && !g_show_tracks ) {
		logg(I, "special track found (", handler_type_, ", '", handler_name_, "')\n");
	}
	do_stretch_ = g_stretch_video && handler_type_ == "vide";

	codec_.parseOk(trak_);
}

void Track::writeToAtoms(bool broken_is_64) {
	Atom *stbl = trak_->atomByName("stbl");
	if (broken_is_64 && stbl->atomByName("stco")){
		stbl->prune("stco");
		Atom *new_co64 = new Atom;
		new_co64->name_ = "co64";
		stbl->children_.push_back(new_co64);
	}
	else if (!broken_is_64 && stbl->atomByName("co64")) {
		stbl->prune("co64");
		Atom *new_stco = new Atom;
		new_stco->name_ = "stco";
		stbl->children_.push_back(new_stco);
	}

	saveSampleTimes();
	saveKeyframes();
	saveSampleSizes();
	saveSampleToChunk();
	saveChunkOffsets();

	editHeaderAtom();

	Atom *tkhd = trak_->atomByName("tkhd");
	HasHeaderAtom::editHeaderAtom(tkhd, getDurationInTimescale(), true);

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
	offsets_.clear();
	if (orig_sizes_.empty()) swap(sizes_, orig_sizes_);
	if (orig_times_.empty()) swap(times_, orig_times_);
	else sizes_.clear();
	keyframes_.clear();
}

void Track::fixTimes() {
	const size_t len_offs = offsets_.size();
	if (codec_.name_ == "samr") {
		times_.clear();
		times_.resize(len_offs, 160);
		return;
	}

	if (times_.empty()) times_ = orig_times_;
	else if (times_.size() < len_offs)
		logg(W, "only found decode times partially!");

	if (times_.empty()) return;

//	cout << codec_.name_ << '\n';
//	for(int i=0; i != 100; i++)
//		cout << "times_[" << i << "] = " << times_[i] << '\n';

	if (times_.size() != len_offs) {
		bool are_different = false; // just a heuristic, which ignores last value
		for (uint i=1; i < min(times_.size(), to_size_t(100)); i++) {
			if (times_[i] != times_[i-1] || times_[times_.size() - (i+1)] != times_[i]) {
				are_different = true;
				break;
			}
		}

		if (are_different) {
			logg(W, "guessed frame durations of '", codec_.name_, "' will probably be wrong!\n");
		}

		while(times_.size() < len_offs) {
			times_.insert(times_.end(), times_.begin(), times_.end());
		}
		times_.resize(len_offs);
	}

	duration_ = 0;
	for (auto t : times_) duration_ += t;
}

vector<int> Track::getSampleTimes() {
	vector<int> sample_times;
	//chunk offsets
	Atom *stts = trak_->atomByNameSafe("stts");

	int entries = stts->readInt(4);
	for(int i = 0; i < entries; i++) {
		int nsamples = stts->readInt(8 + 8*i);
		int time = stts->readInt(12 + 8*i);
		for(int i = 0; i < nsamples; i++)
			sample_times.push_back(time);
	}
	return sample_times;
}

vector<int> Track::getKeyframes() {
	vector<int> sample_key;
	//chunk offsets
	Atom *stss = trak_->atomByName("stss");
	if (!stss) return sample_key;

	int entries = stss->readInt(4);
	for(int i = 0; i < entries; i++)
		sample_key.push_back(stss->readInt(8 + 4*i) - 1);
	return sample_key;
}

vector<int> Track::getSampleSizes() {
	vector<int> sample_sizes;
	//chunk offsets
	Atom *stsz = trak_->atomByNameSafe("stsz");

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



vector<off_t> Track::getChunkOffsets64() {
	vector<off_t> chunk_offsets;
	Atom *co64 = trak_->atomByNameSafe("co64");

	int nchunks = co64->readInt(4);
	for(int i = 0; i < nchunks; i++) {
		int64_t off = co64->readInt64(8 + i*8);
		chunk_offsets.push_back(off);
	}
	return chunk_offsets;
}

vector<off_t> Track::getChunkOffsets() {
	vector<off_t> chunk_offsets;
	//chunk offsets
	Atom *stco = trak_->atomByNameSafe("stco");
	int nchunks = stco->readInt(4);
	for(int i = 0; i < nchunks; i++)
		chunk_offsets.push_back(stco->readInt(8 + i*4));
	return chunk_offsets;
}

vector<int> Track::getSampleToChunk(int n_total_chunks){
	vector<int> sample_to_chunk;

	Atom *stsc = trak_->atomByNameSafe("stsc");

	vector<int> first_chunks;
	auto& off = stsc->cursor_off_;
	off = 4;
	int n_entries = stsc->readInt();
	for (int i=0; i < n_entries; i++) {
		int end_idx = off+12 < stsc->content_.size() ? stsc->readInt(off+12) : n_total_chunks+1;
		int start_idx = stsc->readInt();
		int n_samples = stsc->readInt();
		off += 4;

		for (int i=start_idx; i < end_idx; i++)
			for (int j=0; j < n_samples; j++)
				sample_to_chunk.push_back(i-1);
	}
	return sample_to_chunk;
}

void Track::saveSampleTimes() {
	Atom *stts = trak_->atomByNameSafe("stts");
	vector<pair<int,int>> vp;
	for (uint i = 0; i < times_.size(); i++){
		int v = do_stretch_ ? round(times_[i]*stretch_factor_) : times_[i];
		if (vp.empty() || v != vp.back().second) {  // don't repeat same value
//			cout << times_[i] << " -> " << v << '\n';
			vp.emplace_back(1, v);
		}
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
	if (!keyframes_.size()) {
		trak_->prune("stss");
		return;
	}

	Atom *stss = trak_->atomByNameSafe("stss");

	stss->content_.resize(4 + //version
						  4 + //entries
						  4*keyframes_.size()); //time table
	stss->writeInt(keyframes_.size(), 4);
	for (uint i=0; i < keyframes_.size(); i++)
		stss->writeInt(keyframes_[i] + 1, 8 + 4*i);
}

void Track::saveSampleSizes() {
	Atom *stsz = trak_->atomByNameSafe("stsz");

	stsz->content_.resize(4 + //version
						  4 + //default size
						  4 + //entries
						  4*sizes_.size()); //size table
	stsz->writeInt(0, 4);
	stsz->writeInt(sizes_.size(), 8);
	for (uint i=0; i < sizes_.size(); i++) {
		stsz->writeInt(sizes_[i], 12 + 4*i);
	}
}

void Track::saveSampleToChunk() {
	Atom *stsc = trak_->atomByNameSafe("stsc");

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
	if (co64) {
		co64->content_.resize(4 + //version
		                      4 + //number of entries
		                      8*offsets_.size());
		co64->writeInt(offsets_.size(), 4);
		for (uint i=0; i < offsets_.size(); i++)
			co64->writeInt64(offsets_[i], 8 + 8*i);
	}
	else {
		Atom *stco = trak_->atomByNameSafe("stco");
		stco->content_.resize(4 + //version
		                      4 + //number of entries
		                      4*offsets_.size());
		stco->writeInt(offsets_.size(), 4);
		for (uint i=0; i < offsets_.size(); i++)
			stco->writeInt(offsets_[i], 8 + 4*i);
	}
}

int64_t Track::getDurationInTimescale()  {
	return (int64_t)(double)duration_ * ((double)mp4_timescale_ / (double)timescale_);
}
