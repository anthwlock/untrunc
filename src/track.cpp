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
	constant_size_ = 1;  // for genLikely
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

	Atom *hdlr = trak_->atomByName("hdlr");
	handler_type_ = hdlr->getString(8, 4);

	header_atom_ = trak_->atomByName("mdhd");
	if(!header_atom_)
		throw "No mdhd atom: unknown duration and timescale";
	readHeaderAtom();

	getSampleTimes();
	getKeyframes();
	getSampleSizes();
	getChunkOffsets();
	parseSampleToChunk();
	getCompositionOffsets();

	calcAvgSampleSize();
	setMaxAllowedSampleSize();

	if (constant_duration_ == -1 && !constant_size_ && times_.size() != sizes_.size()) {
		cout << "Mismatch between time offsets and size offsets: \n";
		cout << "Time offsets: " << times_.size() << " Size offsets: " << sizes_.size() << endl;
	}

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
			logg(W2, "using expected ", codec_.name_, " frame size of ", nc, "*", expected_size,
			     ", instead of ", constant_size_, " as found in stsz\n");
			constant_size_ = expected_size;
		}
	}
}

string Track::getCodecNameSlow() {
	Atom *stsd = trak_->atomByName("stsd");
	string name = stsd->getString(12, 4);
	return name.c_str();  // might be smaller than 4
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
	saveCompositionOffsets();

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

	if (codec_.name_ == "mebx") constant_duration_ = 1;
	if (constant_duration_ != -1) {
		duration_ = len_offs * constant_duration_;
		return;
	}

	if (times_.empty()) times_ = orig_times_;
	else if (times_.size() < len_offs)
		logg(W, "only found decode times partially for ", codec_.name_, "!\n");

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
		for (int i = 0; i < entries; i++) {
			uint sz = stsz->readInt(12 + 4*i);
			sizes_.push_back(sz);
		}
		num_samples_ = sizes_.size();
	}
}

void Track::calcAvgSampleSize() {
	if (constant_size_) {
		avg_ss_normal_ = max_ss_normal_ = constant_size_;
		return;
	}

	int min_ss_normal, min_ss_keyframe,
	    max_ss_normal = 0, max_ss_keyframe = 0,
	    avg_ss_normal = 0, avg_ss_keyframe = 0;
	min_ss_normal = min_ss_keyframe = numeric_limits<int>::max();

	auto updateStat = [](int& min_ss, int& avg_ss, int& max_ss, int sz, int n) {
		min_ss = min(min_ss, sz);
		avg_ss = avg_ss + (int)(sz - avg_ss) / n;
		max_ss = max(max_ss, sz);
	};

	uint k = keyframes_.size() ? keyframes_[0] : -1, ik = 0;
	for (uint i=0; i < sizes_.size(); i++) {
		int sz = sizes_[i];
		bool is_keyframe = k == i;

		if (is_keyframe && ++ik < keyframes_.size())
			k = keyframes_[ik];

		int n = i+1;
		if (is_keyframe) updateStat(min_ss_keyframe, avg_ss_keyframe, max_ss_keyframe, sz, n);
		else updateStat(min_ss_normal, avg_ss_normal, max_ss_normal, sz, n);
	}

	avg_ss_normal_   = avg_ss_normal;    min_ss_normal_   = min_ss_normal;    max_ss_normal_   = max_ss_normal;
	avg_ss_keyframe_ = avg_ss_keyframe;  min_ss_keyframe_ = min_ss_keyframe;  max_ss_keyframe_ = max_ss_keyframe;

	int n_keyframe = keyframes_.size();
	int n_total = sizes_.size();
	int n_normal = n_total - n_keyframe;

	min_ss_total_ = min(min_ss_normal_, min_ss_keyframe_);
	avg_ss_total_ = (n_normal * avg_ss_normal_ + n_keyframe * avg_ss_keyframe_) / n_total;
	max_ss_total_ = max(max_ss_normal_, max_ss_keyframe_);
}

void Track::setMaxAllowedSampleSize() {
	int n_total = sizes_.size();
	if (!n_total) {
		max_allowed_ss_ = constant_size_;
		return;
	}

	be_strict_maxpart_ = avg_ss_total_ > (1<<19) - (1<<17) &&
	                     max_ss_normal_ < min_ss_keyframe_ &&
	                     n_total > 35 &&
	                     true;
	logg(V, "ss: ", codec_.name_, " is_stable: ", be_strict_maxpart_, "\n");

	if (!be_strict_maxpart_) {
		int f = 7;
		if (codec_.name_ == "mp4v") f = 4;
		logg(V, "ss: using f=", f, " span\n");
		max_allowed_ss_ = avg_ss_total_ + f*(max_ss_total_ - min_ss_total_);
	}
	else {
		int avg_ss = max(avg_ss_normal_, avg_ss_keyframe_), max_ss = max(max_ss_normal_, max_ss_keyframe_);
		if ((double)avg_ss / max_ss > 0.8) {
			logg(V, "ss: using 2x avg\n");
			max_allowed_ss_ = 2*avg_ss;
		}
		else {
			int f = 2;
			logg(V, "ss: using f=", f, " radius\n");
			max_allowed_ss_ = avg_ss + f*(max_ss - avg_ss);
		}
	}
}

void Track::printDynStats() {
	cout << codec_.name_ << '\n';

	auto printRow3 = [](const string& label, int min, int avg, int max) {
		cout << left << setw(20) << label + ": " << right
		     << setw(12) << min << " "
		     << setw(12) << avg  << " "
		     << setw(12) << max << '\n';
	    };
	printRow3("ss_normal", min_ss_normal_, avg_ss_normal_, max_ss_normal_);
	if (keyframes_.size())
		printRow3("ss_keyframe", min_ss_keyframe_, avg_ss_keyframe_, max_ss_keyframe_);
	printRow3("ss_total", min_ss_total_, avg_ss_total_, max_ss_total_);
	cout << left << setw(20 + 2*(12+1)) << "max_allowed_ss_: " << right
	     << setw(12) << max_allowed_ss_ << " "
	     << "  // strict=" << be_strict_maxpart_<< '\n';
	cout << "chunk_distance_gcd_: " << chunk_distance_gcd_ << '\n';
	cout << "start_off_gcd_: " << start_off_gcd_ << '\n';
	cout << "end_off_gcd_: " << end_off_gcd_ << '\n';

	cout << "likely n_samples/chunk (p=" << likely_n_samples_p << "): ";
	for (uint i=0; i < min(to_size_t(100), likely_n_samples_.size()); i++)
		cout << likely_n_samples_[i] << ' ';

	cout << "\nlikely sample_sizes (p=" << likely_samples_sizes_p << "): ";
	for (uint i=0; i < min(to_size_t(100), likely_sample_sizes_.size()); i++)
		cout << likely_sample_sizes_[i] << ' ';

	int sum = 0;
	for (auto& v : dyn_patterns_) sum += v.size();
	cout << '\n' << "n_mutual_patterns: " << sum << '\n';
	printDynPatterns(true);

	cout << "\n";
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

void Track::getCompositionOffsets() {
	Atom* ctts = trak_->atomByName("ctts");
	if (!ctts) return;

	int entries = ctts->readInt(4);
	for (int i = 0; i < entries; i++) {
		int sample_cnt = ctts->readInt(8 + 8*i);
		int comp_off = ctts->readInt(12 + 8*i);
		orig_ctts_.emplace_back(sample_cnt, comp_off);

		for (int j=0; j < sample_cnt; j++)  // for Mp4::dumpSamples -> Mp4::dumpMatch
			orig_comp_offs_.emplace_back(comp_off);
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
	logg(V, "genPatternPerm of: ", codec_.name_, "\n");
	bool has_twos_transition = false;
	for (uint i=0; i < dyn_patterns_.size(); i++) {
		int max_size = 0;
		double max_e = -1;

		for (auto& p : dyn_patterns_[i]) {
			auto distinct = p.getDistinct();
			auto e = calcEntropy(distinct);
			if (e > max_e) {max_e = e; max_size = distinct.size();}
		}
		if (i == to_uint(g_mp4->twos_track_idx_)) has_twos_transition = true;
		logg(V, i, " ", g_mp4->getCodecName(i), " ", max_e, " ", max_size, "\n");
		perm.emplace_back(i, max_e, max_size);
	}

	sort(perm.begin(), perm.end(), [](const tuple_t& a, const tuple_t& b) {
		if (fabs(get<1>(a) - get<1>(b)) < 0.1) return get<2>(a) > get<2>(b);
		return get<1>(a) > get<1>(b);
	});

	for (uint i=0; has_twos_transition && i < perm.size(); i++) {
		if (get<1>(perm[i]) < 2) {use_looks_like_twos_idx_ = i; break;}
		else if (get<0>(perm[i]) == g_mp4->twos_track_idx_) break;
	}

	dyn_patterns_perm_.clear();
	for (uint i=0; i < perm.size(); i++)
		dyn_patterns_perm_.push_back(get<0>(perm[i]));
}

bool Track::hasZeroTransitions() {
	vector<uchar> b(4, 0x00);
	for (auto& l : dyn_patterns_)
		for (auto& p : l) {
			if (p.hasPattern(Mp4::pat_size_ / 2, b)) return true;
		}
	return false;
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
	stts->content_.resize(4 + //version+flags
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

	stss->content_.resize(4 + //version+flags
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

void Track::saveCompositionOffsets() {  // aka DisplayOffsets
	if (orig_ctts_.size()) {
		int orig_sz = orig_ctts_.size();
//		auto r = findOrder(orig_ctts_, true);
		auto r = findOrder(orig_ctts_);
		logg(V, "reduced origt_ctts_: ", orig_sz, " -> ", orig_ctts_.size(), '\n');
		if (!r) logg(V, "ctts values seem unpredictable..\n");
	}

	if (!orig_ctts_.size() || g_no_ctts) {
		if (g_no_ctts) logg(I, "pruning ctts..\n");
		trak_->prune("ctts");
		return;
	}

	vector<pair<int, int>> vp;
	for (size_t cnt=0; cnt < num_samples_;) {
		for (auto p : orig_ctts_) {
			vp.emplace_back(p);
			cnt += p.first;
			if (cnt >= num_samples_) break;
		}
	}

	Atom *ctts = trak_->atomByNameSafe("ctts");

	ctts->content_.resize(4 + //version+flags
	                      4 + //entries
	                      8*vp.size());
	ctts->seek(4);
	ctts->writeInt(vp.size());
	for (auto p : vp) {
		ctts->writeInt(p.first); // sample_count
		ctts->writeInt(p.second); // composition_offset
	}
}

bool Track::isChunkTrack() {
	return num_samples_ && !sizes_.size();
}

bool Track::hasPredictableChunks() {
	return likely_n_samples_.size() && likely_sample_sizes_.size();
}

bool Track::shouldUseChunkPrediction() {
	return hasPredictableChunks() && (!isSupported() || likely_samples_sizes_p >= 0.99);
}

bool Track::isChunkOffsetOk(off_t off) {
	if (g_mp4->toAbsOff(off) % start_off_gcd_ != 0) return false;

	if (!current_chunk_.off_) return true;
	return (off - current_chunk_.off_) % chunk_distance_gcd_ == 0;
}

int64_t Track::stepToNextOwnChunk(off_t off) {
	auto step = chunk_distance_gcd_ - ((off - current_chunk_.off_) % chunk_distance_gcd_);
	if (!current_chunk_.off_) {
		auto abs_off = g_mp4->toAbsOff(off);
		auto step_abs = chunk_distance_gcd_ - ((abs_off - current_chunk_.off_) % chunk_distance_gcd_);
		step = min(step, step_abs);
	}
	logg(V, "stepToNextOwnChunkOff(", off, "): to: ", codec_.name_,
	     " last chunk_off: ", current_chunk_.off_, " next: ", off + step, "\n");
	return step;
}

int64_t Track::stepToNextOwnChunkAbs(off_t off) {
	if (start_off_gcd_ <= 1) return 0;
	auto abs_off = g_mp4->toAbsOff(off);
	auto step = start_off_gcd_ - (abs_off % start_off_gcd_);

	logg(V, __func__, "(", off, "): from: ", codec_.name_,  " step: ", step, "\n");
	return step;
}

// currently only used for the (dummy) "free" or "jpeg" tracks
int64_t Track::stepToNextOtherChunk(off_t off) {
	if (end_off_gcd_ <= 1) return 0;
	auto abs_off = g_mp4->toAbsOff(off);
	auto step = end_off_gcd_ - (abs_off % end_off_gcd_);

	if (!is_dummy_) {  // jpeg
		if (step == end_off_gcd_) step = 0;
		logg(V, "stepToNextOtherChunkOff(", off, "): from: ", codec_.name_,
		     ", step: ", step, ", next: ", off + step, "\n");
		return step;
	}

	if (!g_mp4->using_dyn_patterns_) return step;

	if (g_mp4->hasUnclearTransitions(ownTrackIdx())) {
		logg(V, "stepToNextOtherChunkOff(", off, "): from: ", codec_.name_,
		     ", step: ", step, ", next: ", off + step, "  // unclear transition!\n");
		return step;
	}

	for (int i=0; i < 5; i++) {
		logg(V, "step:", step, "\n");
		if (g_mp4->wouldMatch(off + step)) {
			logg(V, "stepToNextOtherChunkOff(", off, "): from: ", codec_.name_,
			     ", step: ", step, ", next: ", off + step, "\n");
			return step;
		}
		step += end_off_gcd_;
	}
	logg(V, "stepToNextOtherChunkOff(", off, "): from: ", codec_.name_,  " next: <nothing found>\n");
	return 0;
}

bool Track::chunkMightBeAtAnd() {
	if (!current_chunk_.n_samples_ || likely_n_samples_p < 0.7) return true;
	for (auto n : likely_n_samples_) if (current_chunk_.n_samples_ == n) return true;
	return false;
}

bool Track::chunkReachedSampleLimit() {
	if (likely_n_samples_.size() != 1 || likely_n_samples_p < 0.99) return false;
	return current_chunk_.n_samples_ == likely_n_samples_[0];
}

void Track::printDynPatterns(bool show_percentage) {
	auto own_idx = ownTrackIdx();
	for (uint j=0; j < dyn_patterns_perm_.size(); j++) {
		auto idx = dyn_patterns_perm_[j];
		auto n_total = show_percentage ? g_mp4->chunk_transitions_[{own_idx, idx}].size() : 0;
		if (own_idx == idx && !n_total) continue;
		cout << ss(codec_.name_, "_", g_mp4->getCodecName(idx), " (", own_idx, "->", idx, ") [", dyn_patterns_[idx].size(), "]",
		           (show_percentage ? ss(" (", n_total, ")") : ""), "\n");
		for (auto& p : dyn_patterns_[idx]) {
			if (n_total) cout << ss(fixed, setprecision(3), p.successRate(), " ", p.cnt_, " ");
			cout << p << '\n';
		}
	}
}

bool Track::dummyIsUsedAsPadding() {
	return end_off_gcd_ >= 8;
}

void Track::genLikely() {
	if (likely_n_samples_.size()) return;  // already done
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
		chunk_distance_gcd_ = chunks_[1].off_ - chunks_[0].off_;  // difference makes it independent of "global" offset
		for (uint i=1; i < chunks_.size(); i++) {
			chunk_distance_gcd_ = gcd(chunk_distance_gcd_, chunks_[i].off_ - chunks_[i-1].off_);
		}

		for (auto& c : chunks_) {
			off_t end_off = c.off_ + c.size_;
			end_off_gcd_ = gcd(end_off_gcd_, end_off);
		}

		start_off_gcd_ = chunks_[0].off_;  // these offsets are absolute (to file begin)
		end_off_gcd_ = chunks_[0].off_ + chunks_[0].size_;
		for (auto& c : chunks_) {
			off_t end_off = c.off_ + c.size_;
			start_off_gcd_ = gcd(start_off_gcd_, c.off_);
			end_off_gcd_ = gcd(end_off_gcd_, end_off);
		}
	}
	else {
		chunk_distance_gcd_ = 1;
		start_off_gcd_ = 1;
		end_off_gcd_ = 1;
	}
}

int Track::useDynPatterns(off_t offset) {
	auto buff = g_mp4->getBuffAround(offset, Mp4::pat_size_);
	if (!buff) return -1;

	for (uint i=0; i < dyn_patterns_perm_.size(); i++) {
		if (i == to_uint(use_looks_like_twos_idx_) && Codec::looksLikeTwosOrSowt(buff + Mp4::pat_size_ / 2)) {
			logg(V, "looksLikeTwos: ", codec_.name_, "_", g_mp4->getCodecName(g_mp4->twos_track_idx_), "\n");
			return g_mp4->twos_track_idx_;
		}
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

	if (g_mp4->transitionIsUnclear(ownTrackIdx(), track_idx) && !g_mp4->wouldMatch2(buff + Mp4::pat_size_ / 2)) {
		logg(V, "inverted chunk match: ", codec_.name_, "_", g_mp4->getCodecName(track_idx), "\n");
		return true;
	}

	return false;
}

void Track::applyExcludedToOffs() {
	for (auto& c : chunks_)
		c.off_ -= c.already_excluded_;
}

uint Track::ownTrackIdx() {
	return g_mp4->getTrackIdx(codec_.name_);
}

int64_t Track::getDurationInTimescale()  {
	return duration_ * mp4_timescale_ / timescale_;
}


Track::Chunk::Chunk(off_t off, int64_t size, int ns) : off_(off), size_(size), n_samples_(ns) {}

ostream& operator<<(ostream& out, const Track::Chunk& c) {
	return out << ss(c.off_, ", ", c.size_, ", ", c.n_samples_);
}
