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
#include <random>
#include <iomanip>  // setprecision

extern "C" {
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "atom.h"
#include "mp4.h"

using namespace std;

Track::Track(Atom *trak, AVCodecParameters *c, int ts) : trak_(trak), codec_(c),
    mp4_timescale_(ts) {}

// dummy track
Track::Track(const string& codec_name) {
	codec_.name_ = codec_name;
	is_dummy_ = true;
}

int64_t Track::getNumSamples() const {
	if (sizes_.size()) assert(num_samples_ == sizes_.size());
	return num_samples_;
}

int Track::getSize(size_t idx) {
	return constant_size_ ? constant_size_ : sizes_[idx];
}

int Track::getTime(size_t idx) {
	return constant_duration_ != -1 ? constant_duration_ : times_[idx];
}

int Track::getOrigSize(uint idx) {
	if (constant_size_) return constant_size_;

	if (orig_sizes_.size())
		return orig_sizes_[idx];
	else
		return sizes_[idx];
}

void Track::parseOk() {
	codec_.parseOk(trak_);

	header_atom_ = trak_->atomByName("mdhd");
	if(!header_atom_)
		throw "No mdhd atom: unknown duration and timescale";
	readHeaderAtom();


	getSampleTimes();
	getKeyframes();
	getSampleSizes();

	getChunkOffsets();
	parseSampleToChunk();

	if (constant_duration_ == -1 && !constant_size_ && times_.size() != sizes_.size()) {
		cout << "Mismatch between time offsets and size offsets: \n";
		cout << "Time offsets: " << times_.size() << " Size offsets: " << sizes_.size() << endl;
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

	if (codec_.name_ == "sowt" || codec_.name_ == "twos") {
		assert(constant_size_);

		int nc = codec_.av_codec_params_->channels;
		int expected_size = 2 * nc;

		if (constant_size_ != expected_size) {
			logg(W2, "using expected ", codec_.name_, " frame size of ", nc, "*", expected_size, ", instead of ", constant_size_, " as found in stsz\n");
			constant_size_ = expected_size;
		}
	}
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
	if (orig_sizes_.empty()) swap(sizes_, orig_sizes_);
	else sizes_.clear();

	if (orig_times_.empty()) swap(times_, orig_times_);
	else orig_times_.end();

	keyframes_.clear();
	chunks_.clear();

	num_samples_ = 0;

	if (trak_) trak_->prune("edts");
}

void Track::fixTimes() {
	const size_t len_offs = getNumSamples();

	if (constant_duration_ != -1) {
		duration_ = len_offs * constant_duration_;
		return;
	};

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

void Track::getSampleTimes() {
	Atom *stts = trak_->atomByNameSafe("stts");

	int entries = stts->readInt(4);
	int nsamples1 = stts->readInt(8);
	if (entries == 1 && nsamples1 > 500) {
		constant_duration_ = stts->readInt(12);
		logg(V, "assuming constant duration of ", constant_duration_, " for '", codec_.name_, "' (x", nsamples1, ")\n");
	}
	else {
		for(int i = 0; i < entries; i++) {
			int nsamples = stts->readInt(8 + 8*i);
			int time = stts->readInt(12 + 8*i);
			for(int i = 0; i < nsamples; i++)
				times_.push_back(time);
		}
	}
}

void Track::getKeyframes() {
	Atom *stss = trak_->atomByName("stss");
	if (!stss) return;

	int entries = stss->readInt(4);
	for(int i = 0; i < entries; i++)
		keyframes_.push_back(stss->readInt(8 + 4*i) - 1);
}

void Track::getSampleSizes() {
	Atom *stsz = trak_->atomByNameSafe("stsz");

	int entries = stsz->readInt(8);
	int constant_size = stsz->readInt(4);
	if (constant_size) {
		constant_size_ = constant_size;
		num_samples_ = entries;
	} else {
		for(int i = 0; i < entries; i++)
			sizes_.push_back(stsz->readInt(12 + 4*i));
		num_samples_ = sizes_.size();
	}
}

void Track::getChunkOffsets() {
	Atom* co64 = trak_->atomByName("co64");
	if (co64) {
		int nchunks = co64->readInt(4);
		for(int i = 0; i < nchunks; i++)
			chunks_.emplace_back(co64->readInt64(8 + i*8), -1, -1);
	}
	else {
		Atom *stco = trak_->atomByNameSafe("stco");
		int nchunks = stco->readInt(4);
		for(int i = 0; i < nchunks; i++)
			chunks_.emplace_back(stco->readInt(8 + i*4), -1, -1);
	}
}

void Track::parseSampleToChunk(){
	Atom *stsc = trak_->atomByNameSafe("stsc");

	vector<int> first_chunks;
	auto& off = stsc->cursor_off_;
	off = 4;
	int n_entries = stsc->readInt();
	for (int i=0; i < n_entries; i++) {
		int end_idx = off+12 < stsc->content_.size() ? stsc->readInt(off+12) : chunks_.size()+1;
		int start_idx = stsc->readInt();
		int n_samples = stsc->readInt();
		off += 4;

		for (int i=start_idx; i < end_idx; i++) {
			chunks_[i-1].n_samples_ = n_samples;
		}
	}
}

void Track::genPatternPerm() {
	using tuple_t = tuple<int, double, int>;
	vector<tuple_t> perm;
	for (uint i=0; i < dyn_patterns_.size(); i++) {
		int max_size = 0;
		double max_e = -1;

		for (auto& p : dyn_patterns_[i]) {
			auto distinct = p.getDistinct();
			auto e = calcEntropy(distinct);
			if (e > max_e) {max_e = e; max_size = distinct.size();}
		}
		perm.emplace_back(i, max_e, max_size);
	}

	sort(perm.begin(), perm.end(), [](const tuple_t& a, const tuple_t& b) {
		if (fabs(get<1>(a) - get<1>(b)) < 0.1) return get<2>(a) > get<2>(b);
		return get<1>(a) > get<1>(b);
	});

	dyn_patterns_perm_.clear();
	for (uint i=0; i < perm.size(); i++)
		dyn_patterns_perm_.push_back(get<0>(perm[i]));

}

void Track::saveSampleTimes() {
	Atom *stts = trak_->atomByNameSafe("stts");
	vector<pair<int,int>> vp;

	if (constant_duration_ != -1) {
		vp.emplace_back(getNumSamples(), constant_duration_);
	}
	else {
		for (uint i = 0; i < times_.size(); i++){
			int v = do_stretch_ ? round(times_[i]*stretch_factor_) : times_[i];
			if (!vp.empty() && v == vp.back().second) vp.back().first++;  // don't repeat same value
			else vp.emplace_back(1, v);
		}
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

	Atom *stss = trak_->atomByName("stss");
	if (!stss) {
		stss = new Atom;
		stss->name_ = "stss";
		trak_->children_.push_back(stss);
	}

	stss->content_.resize(4 + //version
						  4 + //entries
						  4*keyframes_.size()); //time table
	stss->writeInt(keyframes_.size(), 4);
	for (uint i=0; i < keyframes_.size(); i++)
		stss->writeInt(keyframes_[i] + 1, 8 + 4*i);
}

void Track::saveSampleSizes() {
	Atom *stsz = trak_->atomByNameSafe("stsz");

	stsz->seek(4);
	if (constant_size_ && num_samples_) {
		stsz->content_.resize(12);
		stsz->writeInt(constant_size_);
		stsz->writeInt(num_samples_);
	}
	else {
		stsz->content_.resize(12 + 4*sizes_.size());
		stsz->writeInt(0);
		stsz->writeInt(sizes_.size());
		for (auto sz : sizes_) stsz->writeInt(sz);
	}
}

void Track::saveSampleToChunk() {
	Atom *stsc = trak_->atomByNameSafe("stsc");
	stsc->seek(8);

	stsc->content_.resize(4 + // version
						  4 + //number of entries
	                      12*chunks_.size());

	int last_ns = -1, num_entries = 0;
	for (uint i=0; i < chunks_.size(); i++) {
		if (last_ns != chunks_[i].n_samples_) {
			stsc->writeInt(i+1);
			stsc->writeInt(chunks_[i].n_samples_);
			stsc->writeInt(1); // todo: stsd related index
			last_ns = chunks_[i].n_samples_;
			num_entries++;
		}
	}
	stsc->writeInt(num_entries, 4);
	stsc->content_.resize(8 + num_entries*12);
}

void Track::saveChunkOffsets() {
	assert(chunks_[0].off_ >= 0);

	Atom *co64 = trak_->atomByName("co64");
	if (co64) {
		co64->seek(4);
		co64->content_.resize(8 + 8*chunks_.size());
		co64->writeInt(chunks_.size());
		for (auto& c : chunks_) co64->writeInt64(c.off_);
	}
	else {
		Atom *stco = trak_->atomByNameSafe("stco");
		stco->seek(4);
		stco->content_.resize(8 + 4*chunks_.size());
		stco->writeInt(chunks_.size());
		for (auto& c : chunks_) stco->writeInt(c.off_);
	}
}

bool Track::hasPredictableChunks() {
	return likely_n_samples_.size() && likely_sample_sizes_.size();
}

bool Track::shouldUseChunkPrediction() {
	return hasPredictableChunks() && (!isSupported() || likely_samples_sizes_p >= 0.99);
}

bool Track::isChunkOffsetOk(off_t off) {
	// chunk-offsets might be regular in respect to absolute file begin, not mdat begin
	if (!current_chunk_.off_ && (g_mp4->current_mdat_->contentStart() + off) % chunk_distance_gcd_ == 0)
		return true;

	return (off - current_chunk_.off_) % chunk_distance_gcd_ == 0;
}

int64_t Track::stepToNextChunkOff(off_t off) {
	auto step = chunk_distance_gcd_ - ((off - current_chunk_.off_) % chunk_distance_gcd_);
	if (!current_chunk_.off_) {
		auto abs_off = off + g_mp4->current_mdat_->contentStart();
		auto step_abs = chunk_distance_gcd_ - ((abs_off - current_chunk_.off_) % chunk_distance_gcd_);
		step = min(step, step_abs);
	}
	logg(V, "stepToNextChunkOff(", off, "): ", codec_.name_,  " last chunk_off: ", current_chunk_.off_, " next: ", off + step, "\n");
	return step;
}

bool Track::chunkMightBeAtAnd() {
	if (!current_chunk_.n_samples_ || likely_n_samples_p < 0.7) return true;
	for (auto n : likely_n_samples_) if (current_chunk_.n_samples_ == n) return true;
	return false;
}

void Track::printDynPatterns(bool show_percentage) {
	auto own_idx = g_mp4->getTrackIdx(codec_.name_);
	for (uint j=0; j < dyn_patterns_perm_.size(); j++) {
		auto idx = dyn_patterns_perm_[j];
		auto n_total = show_percentage ? g_mp4->chunk_transitions_[{own_idx, idx}].size() : 0;
		if (own_idx == idx) continue;
		cout << ss(codec_.name_, "_", g_mp4->tracks_[idx].codec_.name_, " (", own_idx, "->", idx, ") [", dyn_patterns_[idx].size(), "]",
		           (show_percentage ? ss(" (", n_total, ")") : ""), "\n");
		for (auto& p : dyn_patterns_[idx]) {
			if (n_total) cout << ss(fixed, setprecision(3), p.successRate(), " ", p.cnt_, " ");
			cout << p << '\n';
		}
	}
}

void Track::genLikely() {
	assert(sizes_.size() > 0 || constant_size_);

	if (sizes_.size() > 1) {
		random_device rd;
		mt19937 mt(rd());
		auto dis = uniform_int_distribution<size_t>(0, sizes_.size()-2);
		map<int, int> sizes_cnt;
		int n_sample = min(to_size_t(500), sizes_.size());
		for (int n=n_sample; n--;) {
			auto idx = dis(mt);
			sizes_cnt[sizes_[idx]]++;
		}

		for (auto& kv : sizes_cnt) {
			auto size = kv.first, cnt = kv.second;

			auto p = (double)cnt / n_sample;
			if (p >= 0.2) {
				likely_sample_sizes_.push_back(size);
				likely_samples_sizes_p += p;
			}
		}
	}
	else if (constant_size_) {
		likely_sample_sizes_.push_back(constant_size_);
		likely_samples_sizes_p = 1;
	}
	else {
		likely_sample_sizes_.push_back(sizes_[0]);
		likely_samples_sizes_p = 1;
	}


	assert(chunks_.size());

	map<int, int> cnts;
	for (size_t i=0; i < chunks_.size() - 1; i++) cnts[chunks_[i].n_samples_]++;
	for (auto& kv : cnts) {
		auto n_samples = kv.first, cnt = kv.second;
		auto p = (double)cnt / (chunks_.size()-1);
		if (cnts.size() <= 3 || p >= 0.2) {
			likely_n_samples_.push_back(n_samples);
			likely_n_samples_p += p;
		}
	}

	if (chunks_.size() > 1) {
		chunk_distance_gcd_ = chunks_[1].off_ - chunks_[0].off_;
		for (uint i=1; i < chunks_.size(); i++) {
			chunk_distance_gcd_ = gcd(chunk_distance_gcd_, chunks_[i].off_ - chunks_[i-1].off_);
		}
	}
	else chunk_distance_gcd_ = 1;
}

int Track::useDynPatterns(off_t offset) {
	auto buff = g_mp4->getBuffAround(offset, Mp4::pat_size_);
	if (!buff) return -1;

	for (uint i=0; i < dyn_patterns_perm_.size(); i++) {
		auto idx = dyn_patterns_perm_[i];
		if (!g_mp4->tracks_[idx].isChunkOffsetOk(offset)) continue;
		if (doesMatchTransition(buff, idx)) return idx;
	}
	return -1;
}

void Track::genChunkSizes() {
	if (!chunks_.size())
		throw logic_error(ss("healthy file has a '", codec_.name_,
	                         "' track, but no single ", codec_.name_, "-frame!\n"));
	assert(chunks_[0].n_samples_ >= 1);

	if (chunks_[0].n_samples_ == 1) {  // really? better check..
		vector<Track::Chunk> orig_chunks;
		swap(orig_chunks, chunks_);

		size_t sample_idx = 0;
		auto chunkSize = [&](Chunk c){
			int64_t sz = 0;
			for (int i=0; i < c.n_samples_; i++) sz += getSize(sample_idx++);
			return sz;
		};

		auto& c1 = orig_chunks[0];
		c1.size_ = chunkSize(c1);
		for (uint i=0; i+1 < orig_chunks.size(); i++) {
			auto& c2 = orig_chunks[i+1];
			c2.size_ = chunkSize(c2);
			if (c1.off_ + c1.size_ == c2.off_) {
				c1.size_ += c2.size_;
				c1.n_samples_ += c2.n_samples_;
			}
			else {
				chunks_.emplace_back(c1);
				c1 = c2;
			}
		}
		chunks_.push_back(c1);
	}
	else if (chunks_[0].size_ < 0) {  // chunk sizes not yet generated
		int sample_idx = 0;
		for (auto& c : chunks_) {
			c.size_ = 0;
			for (int i=0; i < c.n_samples_; i++)
				c.size_ += getSize(sample_idx++);
		}
	}
}

void Track::pushBackLastChunk() {
	if (!current_chunk_.n_samples_ && is_dummy_) return;  // dummyIsSkippable -> chkOffset -> pushBackLastChunk
	assert(current_chunk_.n_samples_);

	if (is_dummy_ && current_chunk_.size_)
		g_mp4->addUnknownSequence(current_chunk_.off_, current_chunk_.size_);

	chunks_.emplace_back(current_chunk_);
	current_chunk_.n_samples_ = 0;
	current_chunk_.size_ = 0;
	// keep current_chunk_.off_ for stepToNextChunkOff()
}

bool Track::doesMatchTransition(const uchar* buff, int track_idx) {
//	logg(V, codec_.name_, "_", g_mp4->getCodecName(track_idx), '\n');
	for (auto& p : dyn_patterns_[track_idx]) {
//		if (g_log_mode >= LogMode::V) {
//			printBuffer(buff, Mp4::pat_size_);
//			cout << p << '\n';
//		}
		if (p.doesMatch(buff)) {
			return true;
		}
	}
	return false;

}

void Track::applyExcludedToOffs() {
	for (auto& c : chunks_)
		c.off_ -= c.already_excluded_;
}

int64_t Track::getDurationInTimescale()  {
	return duration_ * mp4_timescale_ / timescale_;
}


Track::Chunk::Chunk(off_t off, int64_t size, int ns) : off_(off), size_(size), n_samples_(ns) {}

ostream& operator<<(ostream& out, const Track::Chunk& c) {
	return out << ss(c.off_, ", ", c.size_, ", ", c.n_samples_);
}
