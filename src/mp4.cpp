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
#include <fstream>

extern "C" {
#include <stdint.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

#include "mp4.h"
#include "atom.h"
#include "file.h"

using namespace std;

uint64_t Mp4::step_ = 1;

Mp4::~Mp4() {
	delete root_atom_;
}

void Mp4::parseOk(const string& filename) {
	filename_ok_ = filename;
	auto& file = openFile(filename);

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

	header_atom_ = root_atom_->atomByNameSafe("mvhd");
	readHeaderAtom();

	// https://github.com/FFmpeg/FFmpeg/blob/70d25268c21cbee5f08304da95be1f647c630c15/doc/APIchanges#L86
    #if ( LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58,9,100) )
	av_register_all();
    #endif

	auto ftyp = root_atom_->atomByName("ftyp", true);
	if (ftyp) {
		ftyp_ = ftyp->getString(0, 4);
		logg(V, "ftyp_ = '", ftyp_, "'\n");
	}
	else {
		logg(V, "no 'ftyp' atom found\n");
	}

	if (ftyp_ == "XAVC") {
		logg(V, "detected 'XAVC', deactivating 'g_strict_nal_frame_check'\n");
		g_strict_nal_frame_check = false;
		g_ignore_forbidden_nal_bit = false;
	}

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

void Mp4::parseTracksOk() {
	Codec::initOnce();
	auto mdats = root_atom_->atomsByName("mdat", true);
	if (mdats.size() > 1)
		logg(W, "multiple mdats detected, see '-ia'\n");

	orig_mdat_start_ = mdats.front()->start_;

	auto traks = root_atom_->atomsByName("trak");
	for (uint i=0; i < traks.size(); i++) {
		tracks_.emplace_back(traks[i], context_->streams[i]->codecpar, timescale_);
		auto& track = tracks_.back();
		track.parseOk();

		assert(track.chunks_.size());
		assert(track.chunks_.front().off_ >= mdats.front()->contentStart());
		assert(track.chunks_.back().off_ < mdats.back()->start_ + mdats.back()->length_);
	}

	if (hasCodec("fdsc") && hasCodec("avc1"))
		getTrack("avc1").codec_.strictness_lvl_ = 1;
}


void Mp4::printMediaInfo() {
	printTracks();
	cout << "\n\n";
	printAtoms();
	cout << "\n\n";
	printDynStats();
}

void Mp4::printTracks() {
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

void Mp4::printDynStats() {
	if (first_off_abs_ < 0) genDynStats(true);
	cout << "\ndynamic stats:";
	cout << "\nfirst_off_: " << first_off_abs_;
	cout << "\nfirst_off_rel_: " << first_off_rel_ << '\n';
	for (auto& t : tracks_) {
		cout << t.codec_.name_ << '\n';
		cout << "chunk_distance_gcd_: " << t.chunk_distance_gcd_ << '\n';

		cout << "likely n_samples/chunk (p=" << t.likely_n_samples_p << "): ";
		for (uint i=0; i < min(to_size_t(100), t.likely_n_samples_.size()); i++)
			cout << t.likely_n_samples_[i] << ' ';

		cout << "\nlikely sample_sizes (p=" << t.likely_samples_sizes_p << "): ";
		for (uint i=0; i < min(to_size_t(100), t.likely_sample_sizes_.size()); i++)
			cout << t.likely_sample_sizes_[i] << ' ';

		int sum = 0;
		for (auto& v : t.dyn_patterns_) sum += v.size();
		cout << '\n' << "n_mutual_patterns: " << sum << '\n';
		t.printDynPatterns(true);

		cout << "\n";
	}
}


void Mp4::makeStreamable(const string& ok, const string& output) {
	if (FileRead::alredyExists(output)) {
		logg(W, "destination '", output, "' alredy exists\n");
		hitEnterToContinue();
	}

	parseOk(ok);
	if (!current_mdat_) findMdat(*current_file_);

	auto moov = root_atom_->atomByName("moov");
	auto mdat = root_atom_->atomByName("mdat");
	if (moov->start_ < mdat->start_) {
		logg(I, "alredy streamable!\n");
		return;
	}

	for (auto& t : tracks_) {
		pkt_idx_ += t.sizes_.size();
		for (auto& c : t.chunks_) c.off_ -= mdat->contentStart();
	}

	saveVideo(output);
}

void Mp4::analyzeOffset(const string& filename, off_t real_offset) {
	FileRead file(filename);
	auto mdat = findMdat(file);
	if (real_offset < mdat->contentStart() || real_offset >= mdat->file_end_)
		throw "given offset is not in 'mdat'";

	auto buff = file.getPtrAt(real_offset, 16);
	printBuffer(buff, 16);

	auto off = real_offset - mdat->contentStart();
	auto match = getMatch(off);
	dumpMatch(match, 0);
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
	/* we save all atom except:
	  ctts: composition offset ( we use sample to time)
	  cslg: because it is used only when ctts is present
	  stps: partial sync, same as sync

	  movie is made by ftyp, moov, mdat (we need to know mdat begin, for absolute offsets)
	  assumes offsets in stco are absolute and so to find the relative just subtrack mdat->start + 8
*/

	if (tracks_.back().is_dummy_) tracks_.pop_back();

	if (g_log_mode >= I) {
		cout << "Info: Found " << pkt_idx_ << " packets ( ";
		for(const Track& t : tracks_){
			cout << t.codec_.name_ << ": " << t.getNumSamples() << ' ';
			if (contains({"avc1", "hvc1"}, t.codec_.name_))
				cout << ss(t.codec_.name_, "-keyframes: ", t.keyframes_.size(), " ");
		}
		cout << ")\n";
	}

	chkStrechFactor();
	setDuration();

	for(Track& track : tracks_) {
		track.applyExcludedToOffs();
		track.writeToAtoms(broken_is_64_);

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

	auto& mdat = current_mdat_;
	Atom* original_mdat = root_atom_->atomByName("mdat");
	root_atom_->replace(original_mdat, mdat);
	delete original_mdat;
//	Atom *mdat = root_atom_->atomByName("mdat");

	if (unknown_lengths_.size()) {
		cout << setprecision(4);
		int64_t bytes_not_matched = 0;
		for (auto n : unknown_lengths_) bytes_not_matched += n;
		double percentage = (double)100 * bytes_not_matched / mdat->contentSize();
		logg(W, "Unknown sequences: ", unknown_lengths_.size(), '\n');
		logg(W, "Bytes NOT matched: ", pretty_bytes(bytes_not_matched), " (", percentage, "%)\n");
	}

	if (g_dont_write) return;

	editHeaderAtom();

	Atom *ftyp = root_atom_->atomByName("ftyp");
	Atom *moov = root_atom_->atomByName("moov");

	moov->prune("ctts");
	moov->prune("cslg");
	moov->prune("stps");

	root_atom_->updateLength();

	// remove empty tracks
	for (auto it=tracks_.begin(); it != tracks_.end();) {
		if (!it->getNumSamples()) {
			moov->prune(it->trak_);
			logg(I, "pruned empty '", it->codec_.name_, "' track\n");
			it = tracks_.erase(it);
		}
		else it++;
	}

	//fix offsets
	off_t offset = mdat->headerSize() + moov->length_;
	if(ftyp)
		offset += ftyp->length_; //not all mov have a ftyp.

	for (Track& track : tracks_) {
		assert(track.chunks_.size());
		for (auto& c : track.chunks_) c.off_ += offset;
		track.saveChunkOffsets(); //need to save the offsets back to the atoms
	}

	if (g_dump_repaired) {
		auto dst = filename + ".dump";
		cout << '\n';
		cout << "n_to_dump: " << to_dump_.size() << (to_dump_.size() ? "\n" : " (dumping all)\n");
		logg(I, "dumping to: '", dst, "'\n");
		ofstream f_out(dst);
		cout.rdbuf(f_out.rdbuf());

		filename_ok_ = filename;
		mdat->start_ = offset - 8;  // not mdat->headerSize()
		current_mdat_ = (BufferedAtom*) mdat;
		dumpSamples();
		exit(0);
	}

	//save to file
	logg(I, "saving ", filename, '\n');
	FileWrite file;
	if(!file.create(filename))
		throw "Could not create file for writing: " + filename;

	if(ftyp)
		ftyp->write(file);
	moov->write(file);
	mdat->write(file);
}

string Mp4::getOutputSuffix() {
	string output_suffix;
	if (g_ignore_unknown) output_suffix += ss("-s", Mp4::step_);
	if (g_use_chunk_stats) output_suffix += "-dyn";
	if (g_dont_exclude) output_suffix += "-k";
	if (g_stretch_video) output_suffix += "-sv";
	return output_suffix;
}

void Mp4::chkUntrunc(FrameInfo& fi, Codec& c, int i) {
	auto offset = fi.offset_;
	auto start = loadFragment(offset);

	cout << "\n(" << i << ") Size: " << fi.length_ << " offset: " << offToStr(offset)
	     << "  begin: " << mkHexStr(start, 4) << " " << mkHexStr(start+4, 4)
	     << " end: " << mkHexStr(start+fi.length_-4, 8) << '\n';

	bool ok = true;
	bool matches = false;
	for (auto& t : tracks_)
		if (t.codec_.matchSample(start)){
			if (t.codec_.name_ != c.name_){
				cout << "Matched wrong codec! '" << t.codec_.name_ << "' instead of '" << c.name_ << "'\n";
				ok = false;
			} else {matches = true; break;}
		}
	uint size = c.getSize(start, current_maxlength_, offset);
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
	auto& mdat = current_mdat_;
	if (!mdat) findMdat(*current_file_);
	assert(mdat);

	for (uint idx=0; idx < tracks_.size(); idx++) {
		Track& track = tracks_[idx];
		if (!gen_off_map) cout << "\nTrack " << idx << " codec: " << track.codec_.name_ << endl;

		if (track.shouldUseChunkPrediction()) {
			track.genChunkSizes();
			for (uint i=0; i < track.chunks_.size(); i++) {
				auto& c = track.chunks_[i];
				auto off = c.off_ - mdat->contentStart();
				assert(c.size_ % c.n_samples_ == 0);
				off_to_chunk_[off] = Mp4::Chunk(off, c.n_samples_, idx, c.size_ / c.n_samples_);
			}
		}
		else {
			if (!track.isSupported() && !gen_off_map) continue;
			uint k = track.keyframes_.size() ? track.keyframes_[0] : -1, ik = 0;

			auto c_it = track.chunks_.begin();
			size_t sample_idx_in_chunk = 0;
			off_t cur_off = c_it->off_ - mdat->contentStart();
			for (uint i=0; i < track.sizes_.size(); i++) {
				bool is_keyframe = k == i;

				if (is_keyframe && ++ik < track.keyframes_.size())
					k = track.keyframes_[ik];

				auto sz = track.getSize(i);
				if (cur_off > mdat->contentSize()) {
					logg(W, "reched premature end of mdat\n");
					break;
				}
				auto fi = FrameInfo(idx, is_keyframe, track.getTime(i), cur_off, sz);

				if (gen_off_map) off_to_frame_[cur_off] = fi;
				else chkUntrunc(fi, track.codec_, i);

				if (++sample_idx_in_chunk >= to_uint(c_it->n_samples_)) {
					cur_off = (++c_it)->off_ - mdat->contentStart();
					sample_idx_in_chunk = 0;
				}
				else
					cur_off += sz;
			}
		}
	}
}


void Mp4::genChunks() {
	for (auto& t : tracks_) t.genChunkSizes();
}

void Mp4::genChunkTransitions() {
	vector<uint> cur_chunk_idx(tracks_.size());
	int last_track_idx = -1;

	tracks_.emplace_back("free");
	tracks_.back().constant_size_ = 1;  // for genLikely
	idx_free_ = tracks_.size()-1;
	logg(V, "created dummy track 'free'\n");

	vector<pair<int, int>> order;
	bool order_finished = false;
	size_t chunk_idx = 0, first_failed = 0;

	while (true) {
		int track_idx = -1;
		auto off = numeric_limits<off_t>::max();
		for (uint i=0; i < cur_chunk_idx.size(); i++) {
			if (cur_chunk_idx[i] >= tracks_[i].chunks_.size()) continue;
			auto toff = tracks_[i].chunks_[cur_chunk_idx[i]].off_;
			if (toff < off) {
				track_idx = i;
				off = toff;
			}
		}
		if (track_idx < 0) break;

		auto& cur_chunk = tracks_[track_idx].chunks_[cur_chunk_idx[track_idx]];
		if (!order_finished && order.size() < 25) {
			auto p = make_pair(track_idx, cur_chunk.n_samples_);
			if (order.size() && order.front() == p) order_finished = true;
			else order.emplace_back(p);
		}
		else if (order_finished && !first_failed) {
			auto p = make_pair(track_idx, cur_chunk.n_samples_);
			if (order[chunk_idx % order.size()] != p) first_failed = chunk_idx;
		}

		if (chunk_idx == 0) {
			first_off_abs_ = cur_chunk.off_;
			first_off_rel_ = first_off_abs_ - current_mdat_->contentStart();
			orig_first_track_ = &tracks_[track_idx];
		}

		if (last_track_idx >= 0) {
			auto& last_chunk = tracks_[last_track_idx].chunks_[cur_chunk_idx[last_track_idx]-1];
			auto last_end = last_chunk.off_ + last_chunk.size_;
			assert(off - current_mdat_->contentStart() >= pat_size_ / 2);
			assert(off >= last_end);
			if (off != last_end) {
				chunk_transitions_[{last_track_idx, idx_free_}].emplace_back(last_end);
				tracks_[idx_free_].chunks_.emplace_back(last_end, off - last_end, off - last_end);
				chunk_transitions_[{idx_free_, track_idx}].emplace_back(off);
			}
			else {
				chunk_transitions_[{last_track_idx, track_idx}].emplace_back(off);
			}
		}
		last_track_idx = track_idx;
		cur_chunk_idx[track_idx]++;
		chunk_idx++;
	}

	if (!tracks_.back().chunks_.size()) {
		tracks_.pop_back();
		idx_free_ = kDefaultFreeIdx;
		logg(V, "removed dummy track 'free'\n");
	}

	if (g_log_mode >= V) {
		cout << "first_failed: " << first_failed << " of " << chunk_idx << '\n';
		cout << "order_finished: " << order_finished << '\n';
		cout << "order: ";
		for (auto& p : order) cout << ss("(", p.first, ", ", p.second, ") ");
		cout << '\n';
	}
	if (order_finished && (!first_failed || chunk_idx - first_failed <= order.size()))  // last chunks may be shorter
		track_order_ = order;
}

buffs_t Mp4::offsToBuffs(const offs_t& offs, const string& load_prefix) {
	int cnt=0;
	buffs_t buffs;
	for (auto off : offs) {
		if (g_log_mode == I) outProgress(cnt++, offs.size(), load_prefix);
		auto buff = current_file_->getFragment(off - pat_size_/2, pat_size_);
		buffs.emplace_back(buff, buff+pat_size_);
	}
	if (g_log_mode == I) cout << string(20, ' ') << '\r';
	return buffs;
}

patterns_t Mp4::offsToPatterns(const offs_t& all_offs, const string& load_prefix) {
	auto offs_to_consider = choose100(all_offs);
	auto buffs = offsToBuffs(offs_to_consider, load_prefix);
	auto patterns = genRawPatterns(buffs);

	auto offs_to_check = choose100(all_offs);
	auto buffs2 = offsToBuffs(offs_to_consider, load_prefix);
	countPatternsSuccess(patterns, buffs2);

//	for (auto& p : patterns) cout << p.successRate() << " " << p << '\n';

	filterBySuccessRate(patterns, load_prefix);
	return patterns;
}

void Mp4::genDynPatterns() {
	for (auto& t : tracks_) t.dyn_patterns_.resize(tracks_.size());

	for (auto const& kv: chunk_transitions_) {
		auto& patterns = tracks_[kv.first.first].dyn_patterns_[kv.first.second];
		string prefix = ss(kv.first.first, "->", kv.first.second, ": ");

		patterns = offsToPatterns(kv.second, prefix);
	}
}


void Mp4::noteUnknownSequence(off_t offset) {
	addToExclude(offset-unknown_length_, unknown_length_);
	unknown_lengths_.emplace_back(unknown_length_);
	unknown_length_ = 0;
}

void Mp4::addUnknownSequence(off_t start, uint64_t length) {
	assert(length);
	addToExclude(start, length);
	unknown_lengths_.emplace_back(length);
}

void Mp4::addToExclude(off_t start, uint64_t length, bool force) {
	if (g_dont_exclude && !force) return;
	assert(!current_mdat_->sequences_to_exclude_.size() ||
	       start > current_mdat_->sequences_to_exclude_.back().first);

	if (start + length > to_uint(current_mdat_->contentSize())) {
		logg(W, "addToExclude: sequence goes beyond EOF\n");
		length = start - current_mdat_->contentSize();
	}

	current_mdat_->sequences_to_exclude_.emplace_back(start, length);
	current_mdat_->total_excluded_yet_ += length;
}

FileRead& Mp4::openFile(const string& filename) {
	delete current_file_;
	current_file_ = new FileRead(filename);
	if (!current_file_->length())
		throw length_error(ss("zero-length file: ", filename));
	return *current_file_;
}

void Mp4::dumpIdxAndOff(off_t off, int idx) {
	auto real_off = current_mdat_->contentStart() + off;
	cout << setw(15) << ss("(", idx++, ") ") << setw(12) << ss(off, " / ")
	     << setw(8) << real_off << " : ";
}

void Mp4::dumpMatch(const FrameInfo& fi, int idx) {
	dumpIdxAndOff(fi.offset_, idx);
	cout << fi << '\n';
}

void Mp4::dumpChunk(const Mp4::Chunk& chunk, int& idx) {
	dumpIdxAndOff(chunk.off_, idx);
	cout << chunk << '\n';
	idx += chunk.n_samples_;
}

bool Mp4::dummyIsSkippable() {
	auto& t = tracks_[idx_free_];
	logg(V, "running dummyIsSkippable() ... \n");
//	offs_t all_offs;
	vector<pair<off_t, off_t>> tests;
//	for (auto& c : t.chunks_) tests.emplace_back(c.off_, c.off_+c.size_);
	for (auto& c : t.chunks_) tests.emplace_back(c.off_ - current_mdat_->contentStart(), c.off_+c.size_ - current_mdat_->contentStart());
	auto chosen_tests = choose100(tests);
	logg(V, "chosen_tests.size(): ", chosen_tests.size(), '\n');
	for (auto& kv : tests) {
		off_t off = kv.first, off_end = kv.second;
		chkOffset(off);
//		if (off != off_end) return false;
		if (off != off_end) {
			logg(V, "dumyIsSkippable = false\n");
			return false;
		}
	}
	logg(V, "dumyIsSkippable = true\n");
	return true;
}

void Mp4::correctChunkIdx(int track_idx) {
	assert(track_idx >= 0 && track_idx != idx_free_);
	if (!track_order_.size()) return;

	// TODO: call correctChunkIdx once we know n_samples
	//       ATM result could be wrong if 'chunk::ĺikely_n_samples_.size() > 1'
	while (track_order_[chunk_idx_].first != track_idx) chunk_idx_++;

	if (tracks_[track_idx].likely_n_samples_.size() > 1)
		logg(W, "correctChunkIdx(", track_idx, ") could be wrong");
}

bool Mp4::isTrackOrderEnough() {
	if (!track_order_.size()) return false;
	for (auto& t : tracks_) {
		if (t.isSupported()) continue;
		if (t.is_dummy_ && dummyIsSkippable()) continue;
		if (!t.is_dummy_ && t.likely_sample_sizes_.size() == 1) continue;
	}
	return true;
}

void Mp4::genDynStats(bool force_patterns) {
	if (chunk_transitions_.size()) return;  // alredy generated
	if (!current_mdat_) findMdat(*current_file_);
	genChunks();

	genChunkTransitions();
	for (auto& t: tracks_) t.genLikely();
	bool is_enough = isTrackOrderEnough();
	logg(force_patterns ? I : V, "isTrackOrderEnough: ", is_enough, "  (sz=", track_order_.size(), ")\n");
	if (is_enough && !force_patterns) return;
	else if (track_order_.size()) {
		// in theory this could probably still be used somehow..
		logg(W, "track_order_ found, but not sufficient\n");
		track_order_.clear();
	}

	genDynPatterns();
	for (auto& t: tracks_) t.genPatternPerm();
}

void Mp4::checkForBadTracks() {
	if (track_order_.size()) return;  // we alredy checked via `isTrackOrderEnough()`
	for (auto& t: tracks_) {
		if (!t.hasPredictableChunks() && !t.codec_.isSupported()) {
			logg(E, "bad track: '", t.codec_.name_, "'\n");
			exit(1);
		}
	}
}

void Mp4::dumpSamples() {
	auto& mdat = current_mdat_;
	if (!mdat) {
		auto& file = openFile(filename_ok_);
		findMdat(file);
	}
	if (needDynStats())
		for (auto& t : tracks_) t.genLikely();
	analyze(true);
	cout << filename_ok_ << '\n';

	int i = 0;
	if (to_dump_.size())
		for (auto const& x : to_dump_)
			dumpMatch(x, i++);
	else {
		auto frame_it = off_to_frame_.begin();
		auto chunk_it = off_to_chunk_.begin();
		auto frameItAtEnd = [&](){return frame_it == off_to_frame_.end();};
		auto chunkItAtEnd = [&](){return chunk_it == off_to_chunk_.end();};
		while (true) {
			if (!chunkItAtEnd() && !frameItAtEnd()) {
				if (frame_it->second.offset_ < chunk_it->second.off_)
					dumpMatch((frame_it++)->second, i++);
				else
					dumpChunk((chunk_it++)->second, i);
			}
			else if (!frameItAtEnd())
				dumpMatch((frame_it++)->second, i++);
			else if (!chunkItAtEnd())
				dumpChunk((chunk_it++)->second, i);
			else
				break;
		}
	}
}

BufferedAtom* Mp4::findMdat(FileRead& file_read) {
	delete current_mdat_;
	auto& mdat = current_mdat_ = new BufferedAtom(file_read);
	mdat->name_ = "mdat";

	Atom atom;

	if (!isPointingAtAtom(file_read)) {
		logg(W, "no mp4-structure found in: '", file_read.filename_, "'\n");
		if (ftyp_ == "XAVC") {
			logg(V, "using orig_mdat_start_ (== ", orig_mdat_start_, ")\n");
			atom.start_ = orig_mdat_start_;
			atom.name_ = "mdat";
		}
		else if (!g_search_mdat) {
			logg(I, "assuming start_offset=0. \n",
			     "      use '-sm' to search for 'mdat' atom instead (via brute-force)\n");
			mdat->start_ = -8;
			atom.name_ = "mdat";
		}
	}

	while(atom.name_ != "mdat") {
		off_t new_pos = Atom::findNextAtomOff(file_read, &atom, true);
		if (new_pos >= file_read.length() || new_pos < 0) {
			logg(W, "start of mdat not found\n");
			atom.start_ = -8;  // no header
			file_read.seek(0);
			break;
		}
		file_read.seek(new_pos);
		atom.parseHeader(file_read);
	}
	mdat->start_ = atom.start_;
	mdat->file_end_ = file_read.length();

	return mdat;
}

void Mp4::addFrame(const FrameInfo& fi) {
	Track& track = tracks_[fi.track_idx_];
	track.num_samples_++;

	if (fi.keyframe_)
		track.keyframes_.push_back(track.sizes_.size());

	if (fi.should_dump_)
		to_dump_.emplace_back(fi);

	if (fi.audio_duration_ && track.constant_duration_ == -1) track.times_.push_back(fi.audio_duration_);
	if (!track.constant_size_) track.sizes_.push_back(fi.length_);
}

const uchar* Mp4::loadFragment(off_t offset, bool update_cur_maxlen) {
	if (update_cur_maxlen)
		current_maxlength_ = min((int64_t) g_max_partsize, current_mdat_->contentSize() - offset);
	auto buf_sz = min((int64_t) g_max_buf_sz_needed, current_mdat_->contentSize() - offset);
	return current_fragment_ = current_mdat_->getFragment(offset, buf_sz);
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

bool Mp4::pointsToZeros(off_t off) {
	if (current_mdat_->contentSize() - off < 4) return false;
	auto buff = current_mdat_->getFragment(off, 4);
	if (*(int*)buff == 0) {
		logg(V, "pointsToZeros: found 4 zero bytes at ", off, "\n");
		return true;
	}
	return false;
}

bool Mp4::shouldBeStrict(off_t off, int track_idx) {
	auto& t = tracks_[track_idx];
	if (!unknown_length_ || !t.isSupported()) return false;
	if (!g_use_chunk_stats || !tracks_.back().is_dummy_ || !t.chunkMightBeAtAnd() || !t.isChunkOffsetOk(off)) return true;

	assert(last_track_idx_ == idx_free_);
	auto buff = getBuffAround(off, Mp4::pat_size_);
	if (!buff) return true;

	for (auto& p : tracks_.back().dyn_patterns_[track_idx]) {
		if (p.doesMatch(buff)) {
			logg(V, "won't be strict since a free_", t.codec_.name_, " pattern matched at ", off, "\n");
			return false;
		}
	}
	return true;

}

bool Mp4::wouldMatch(off_t offset, const string& skip, bool force_strict, int last_track_idx) {
	auto chkChunkOffOk = [&](uint i) -> bool {
		if (g_use_chunk_stats && to_uint(last_track_idx) != i && !tracks_[i].isChunkOffsetOk(offset)) {
			logg(V, "would match, but offset is not accepted as start-of-chunk by '", tracks_[i].codec_.name_, "'\n");
			return false;
		}
		else logg(V, "chunkOffOk: ", offset, " ok for ", tracks_[i].codec_.name_, "\n");
		return true;
	};

	auto start = loadFragment(offset);
	if (idx_free_ >= 0 && pointsToZeros(offset)) {
		if (tracks_[last_track_idx].doesMatchTransition(start, idx_free_)) {
			logg(V, "wouldMatch(", offset, ", \"", skip, "\", ", force_strict, ") -> yes, ", getCodecName(last_track_idx), "_free\n");
			return chkChunkOffOk(idx_free_);
		}
		return false;
	}

	for (uint i=0; i < tracks_.size(); i++) {
		auto& c = tracks_[i].codec_;
		bool be_strict = force_strict || shouldBeStrict(offset, i);
		if (tracks_[i].codec_.name_ == skip) continue;
		if (be_strict && !c.matchSampleStrict(start)) continue;
		if (!be_strict && !c.matchSample(start)) continue;

		logg(V, "wouldMatch(", offset, ", \"", skip, "\", ", force_strict, ") -> yes, ", c.name_, "\n");
		return chkChunkOffOk(i);
	}

	if (g_use_chunk_stats) {
		auto last_idx = last_track_idx >= 0 ? last_track_idx : last_track_idx_;
		if (last_idx < 0) return false;
		if (wouldMatchDyn(offset, last_idx)) {
			logg(V, "wouldMatch(", offset, ", \"", skip, "\", ", force_strict, ") -> yes\n");
			return true;
		}
	}

	logg(V, "wouldMatch(", offset, ", \"", skip, "\", ", force_strict, ") -> no\n");
	return false;
}


FrameInfo Mp4::getMatch(off_t offset, bool force_strict) {
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
		if (g_use_chunk_stats && (to_uint(last_track_idx_) != i || unknown_length_) && !track.isChunkOffsetOk(offset)) {
			logg(V, "offset not accepted as start-of-chunk offset by '", track.codec_.name_, "'\n");
			continue;
		}

		bool be_strict = force_strict || shouldBeStrict(offset, i);

		if (be_strict && !c.matchSampleStrict(start)) continue;
		if (!be_strict && !c.matchSample(start)) continue;

		int length_signed = c.getSize(start, current_maxlength_, offset);
		uint length = static_cast<uint>(length_signed);

		logg(V, "part-length: ", length_signed, '\n');
		if(length_signed < 1) {
			logg(V, "Invalid length: part-length is ", length_signed, '\n');
			continue;
		}
		if(length > current_maxlength_) {
			logg(V, "limit: ", min(g_max_partsize, current_maxlength_), "\n");
			logg(E, "Invalid length: ", length, " - too big (track: ", i, ")\n");
			continue;
		}
		if (c.was_bad_) continue;

		return FrameInfo(i, c, offset, length);
	}

	return FrameInfo();
}

bool Mp4::wouldMatchDyn(off_t offset, int last_idx) {
	auto new_track = tracks_[last_idx].useDynPatterns(offset);
	if (new_track >= 0) {
		logg(V, "wouldMatchDyn(", offset, ", ", last_idx, ") -> yes (", getCodecName(last_idx), "_", getCodecName(new_track), ")\n");
		return true;
	}
	logg(V, "wouldMatchDyn(", offset, ", ", last_idx, ") -> no\n");
	return false;
}

bool Mp4::anyPatternMatchesHalf(off_t offset, uint track_idx_to_try) {
	auto buff = g_mp4->current_mdat_->getFragment(offset, Mp4::pat_size_ / 2);
	if (!buff) return false;

	for (auto& t : tracks_) {
		for (auto& p : t.dyn_patterns_[track_idx_to_try]) {
			if (g_log_mode >= V) {
				cout << string(36, ' ');
				printBuffer(buff, Mp4::pat_size_);
				cout << p << '\n';
			}
			if (p.doesMatchHalf(buff)) return true;
		}
	}
	return false;

}

Mp4::Chunk Mp4::fitChunk(off_t offset, uint track_idx_to_fit, uint known_n_samples) {
	Mp4::Chunk c;
	auto& t = tracks_[track_idx_to_fit];
	if (!t.hasPredictableChunks()) return c;

	for (auto n_samples : t.likely_n_samples_) {
		if (known_n_samples) n_samples = known_n_samples;
		for (auto s_sz : t.likely_sample_sizes_) {
			auto dst_off = offset + n_samples*s_sz;
			if (dst_off < current_mdat_->contentSize() && wouldMatch(dst_off, "", false, track_idx_to_fit)) {
				assert(n_samples > 0);
				c = Chunk(offset, n_samples, track_idx_to_fit, s_sz);
				return c;
			}
		}
		if (known_n_samples) break;
	}
	return c;
}

int Mp4::getLikelyNextTrackIdx(int* n_samples) {
	auto p = track_order_[chunk_idx_ % track_order_.size()];
	if (n_samples) *n_samples = p.second;
	return p.first;
}

Mp4::Chunk Mp4::getChunkPrediction(off_t offset, bool only_perfect_fit) {
	logg(V, "called getChunkPrediction(", offToStr(offset), ") ... \n");
	Mp4::Chunk c;
	if (last_track_idx_ == kDefaultFreeIdx) return c;  // could try all instead..
	int track_idx;
	if (track_order_.size()) {
		int n_samples;
		track_idx = getLikelyNextTrackIdx(&n_samples);
		auto& track = tracks_[track_idx];
		if (!track.hasPredictableChunks()) return c;
		else if (track.likely_sample_sizes_.size() > 1) {
			c = fitChunk(offset, track_idx, n_samples);
			if (!c) logg(V, "fitChunk() failed despite supposedly known (track_idx, n_samples) = ", track_idx, ", ", n_samples, "\n");
			return c;
		}
		else {
			auto s_sz = track.likely_sample_sizes_[0];
			if (n_samples * s_sz > current_mdat_->contentSize() - offset) return c;
			c = Chunk(offset, n_samples, track_idx, s_sz);
			logg(V, "chunk derived from track_order_:", c, "\n");
			return c;
		}

	}

	if (last_track_idx_ == -1) {  // very first offset
		track_idx = getTrackIdx(orig_first_track_->codec_.name_);
		cout << "orig_trak:" << orig_first_track_->codec_.name_ << " " << track_idx << '\n';
		if (!anyPatternMatchesHalf(offset, track_idx)) {
			cout << "no pattern suggests this..\n";
			return c;
		}
	}
	else {
		track_idx = tracks_[last_track_idx_].useDynPatterns(offset);
	}

//	auto track_idx = tracks_[last_track_idx_].useDynPatterns(offset);
	if (track_idx >= 0 && !tracks_[track_idx].shouldUseChunkPrediction()) {
		logg(V, "should not use chunk prediction for '", getCodecName(track_idx), "'\n");
		return c;
	}
	else if (track_idx < 0) {
//		logg(V, "no chunk with future found\n");
		logg(V, "found no plausible chunk-transition pattern-match\n");
		return c;
	}

	auto& t = tracks_[track_idx];
	logg(V, "transition pattern ", getCodecName(last_track_idx_), "_", t.codec_.name_, " worked\n");

	if ((c = fitChunk(offset, track_idx))) {
		logg(V, "chunk found: ", c, "\n");
		return c;
	}

	if (only_perfect_fit) return c;

	// we boldly assume that track_idx was correct
	auto s_sz = t.likely_sample_sizes_[0];
	auto n_samples = t.likely_n_samples_[0];  // smallest sample_size
	if (t.likely_n_samples_p < 0.9) {
		const int min_sz = 64;  // min assumed chunk size
		int new_n_samples = max(1, min_sz / s_sz);
		logg(V, "reducing n_sample ", n_samples, " -> ", new_n_samples, " because unsure\n");
		n_samples = new_n_samples;
	}
	n_samples = min(n_samples, int((current_mdat_->contentSize() - offset) / s_sz));
	c = Chunk(offset, n_samples, track_idx, s_sz);

	assert(c.track_idx_ >= 0 && to_size_t(c.track_idx_) < tracks_.size());

	bool at_end = n_samples < t.likely_n_samples_[0];
	if (!at_end)
		logg(W2, "found chunk has no future: ", c, " at ", offToStr(c.off_ + c.size_), "\n");

	return c;
}

const uchar* Mp4::getBuffAround(off_t offset, int64_t n) {
	auto& mdat = g_mp4->current_mdat_;
	if (offset - n/2 < 0 || offset + n/2 > mdat->contentSize()) return nullptr;
	return mdat->getFragment(offset - n/2, n);
}

bool Mp4::needDynStats() {
	if (g_use_chunk_stats) return true;
	for (auto& t: tracks_)
		if (!t.isSupported()) {
			logg(I, "unknown track '", t.codec_.name_, "' found -> fallback to dynamic stats\n");
			return true;
		}
	return false;
}

bool Mp4::chkNeedOldApi() {
	if (!is_new_ffmpeg_api) return false;
	vector<string> v = {"mp4v"};
	for (auto& t: tracks_)
		if (contains(v, t.codec_.name_)) {
			logg(E, "track '", t.codec_.name_, "' is not supported with ffmpeg > 3.3!\n");
			return true;
		}
	return false;
}

FrameInfo::FrameInfo(int track_idx, Codec& c, off_t offset, uint length)
    : track_idx_(track_idx), keyframe_(c.was_keyframe_), audio_duration_(c.audio_duration_),
      offset_(offset), length_(length), should_dump_(c.should_dump_) {}

FrameInfo::FrameInfo(int track_idx, bool was_keyframe, uint audio_duration, off_t offset, uint length)
    : track_idx_(track_idx), keyframe_(was_keyframe), audio_duration_(audio_duration),
      offset_(offset), length_(length), should_dump_(false) {}

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

Mp4::Chunk::Chunk(off_t off, int ns, int track_idx, int sample_size)
    : Track::Chunk (off, ns * sample_size, ns), track_idx_(track_idx), sample_size_(sample_size) {}

ostream& operator<<(ostream& out, const Mp4::Chunk& c) {
	return out << ss("'", g_mp4->getCodecName(c.track_idx_), "' (", c.sample_size_, " x", c.n_samples_, ")");
}

bool operator==(const Mp4::Chunk& a, const Mp4::Chunk& b) {
	return a.off_ == b.off_ && a.n_samples_  == b.n_samples_ &&
	        a.track_idx_ == b.track_idx_ && a.n_samples_ == b.n_samples_ &&
	        a.size_ == b.size_;
}
bool operator!=(const Mp4::Chunk& a, const Mp4::Chunk& b) { return !(a == b); }


int64_t Mp4::calcStep(off_t offset) {
	int64_t step = numeric_limits<int64_t>::max();
	if (g_use_chunk_stats) {
		step = numeric_limits<int64_t>::max();
		for (auto& t : tracks_) {
			if (t.is_dummy_) continue;
			step = min(step, t.stepToNextChunkOff(offset));
		}

		step = min(step, current_mdat_->contentSize() - offset);
	}
	else {
		step = Mp4::step_;
	}
	return step;
}

bool isAllZeros(const uchar* buf, int n) {
	for (int i=0; i < n; i+=4) if (*(int*)(buf+i)) return false;
	return true;
}

bool Mp4::isAllZerosAt(off_t off, int n) {
	if (current_mdat_->contentSize() - off < n) return false;
	auto buff = current_mdat_->getFragment(off, 4);
	if (isAllZeros(buff, n)) {
		logg(V, "isAllZerosAt: found ", n, " zero bytes at ", off, "\n");
		return true;
	}
	return false;
}

bool Mp4::chkOffset(off_t& offset) {
start:
	if (offset >= current_mdat_->contentSize()) {  // at end?
		if (last_track_idx_ >= 0)
			tracks_[last_track_idx_].pushBackLastChunk();
		if (unknown_length_) noteUnknownSequence(offset);
		return false;
	}


	auto start = loadFragment(offset);
	uint begin = *(uint*)start;

	static uint loop_cnt = 0;
	if (g_log_mode == I && loop_cnt++ % 2000 == 0) outProgress(offset, current_mdat_->file_end_);

	auto shouldIgnoreZeros = [&]() {
		if (g_use_chunk_stats) {
			for (auto& cn : {"sowt", "twos"}) {
				if (hasCodec(cn) && getTrack(cn).isChunkOffsetOk(offset) && !isAllZeros(start+4, 32-4)) {
					logg(V, "won't skip zeros at: ", offToStr(offset), "\n");
					return true;
				}
			}
			for (auto& cn : {"rtmd"}) {
				if (hasCodec(cn) && getChunkPrediction(offset, true)) {
					logg(V, "won't skip zeros at: ", offToStr(offset), " (perfect chunk-match)\n");
					return true;
				}
			}
		}
		return false;
	};

	if (*(int*)start == 0 && !shouldIgnoreZeros()) {
		logg(V, "skipping zeros at: ", offToStr(offset), "\n");
		int64_t step = 4;
		if (unknown_length_ || g_use_chunk_stats) step = calcStep(offset);
		if (unknown_length_) unknown_length_ += step;
		else if (tracks_[last_track_idx_].is_dummy_) {
			auto& c = tracks_[idx_free_].current_chunk_;
			c.size_ += step;
			c.n_samples_ += step;  // sample size is 1
		}
		offset += step;
		goto start;
	}

	// skip fake moov
	if (string(start+4, start+8) == "moov") {
		if (unknown_length_) noteUnknownSequence(offset);
		uint moov_len = swap32(begin);
		addToExclude(offset, moov_len, true);
		logg(V, "Skipping moov atom: ", moov_len, '\n');
		offset += moov_len;
		goto start;
	}

	// skip free atoms
	if (string(start+4, start+8) == "free") {
		if (unknown_length_) noteUnknownSequence(offset);
		if (last_track_idx_ >= 0)
			tracks_[last_track_idx_].pushBackLastChunk();
		if (idx_free_ >= 0) last_track_idx_ = idx_free_;

		uint atom_len = swap32(begin);
		addToExclude(offset, atom_len);
		logg(V, "Skipping 'free' atom: ", atom_len, " at: ", offToStr(offset), '\n');
		offset += atom_len;
		goto start;
	}

	// skip 'mdat' headers
	if (string(start+4, start+8) == "mdat") {
		if (unknown_length_) noteUnknownSequence(offset);
		if (last_track_idx_ >= 0)
			tracks_[last_track_idx_].pushBackLastChunk();
		if (idx_free_ >= 0) last_track_idx_ = idx_free_;

		addToExclude(offset, 8);
		logg(V, "Skipping 'mdat' header: ", offToStr(offset), '\n');
		offset += 8;
		goto start;
	}

	if (g_log_mode >= LogMode::V) {
		logg(V, "\n(reading element from mdat)\n");
		printOffset(offset);
	}

	return true;
}

void Mp4::printOffset(off_t offset) {
	auto s = current_mdat_->getFragment(offset, min((int64_t)8, current_mdat_->contentSize() - offset));

	uint begin = swap32(*(uint*)s);
	uint next = swap32(*(uint*)(s+4));
	logg(V, "Offset: ", offToStr(offset), " : ", setfill('0'), setw(8), hex, begin, " ", setw(8), next, dec, '\n');
}


void Mp4::chkFrameDetectionAt(FrameInfo& detected, off_t off) {
	auto& correct = off_to_frame_[off];
	if (correct != detected) {
		cout << "bad detection (at " << offToStr(off) << ", pkt " << pkt_idx_
		     << ", chunk " << tracks_[correct.track_idx_].chunks_.size()
		     << ", pkt_in_chunk " << tracks_[correct.track_idx_].current_chunk_.n_samples_
		     << "):\n"
		     << "detected: " << detected << '\n'
		     << "correct : " << correct << '\n';
		hitEnterToContinue();
	}
}

void Mp4::chkChunkDetectionAt(Mp4::Chunk& detected, off_t off) {
	auto& correct = off_to_chunk_[off];
	if (correct != detected) {
		cout << "bad chunk detection (at " << offToStr(off) << ", pkt " << pkt_idx_
		     << ", chunk " << tracks_[correct.track_idx_].chunks_.size()
		     << "):\n"
		     << "detected: " << detected << '\n'
		     << "correct : " << correct << '\n';
		hitEnterToContinue();
	}
}

bool Mp4::tryChunkPrediction(off_t& offset) {
	if (!g_use_chunk_stats) return false;

	Mp4::Chunk chunk = getChunkPrediction(offset);
	if (chunk) {
		auto& t = tracks_[chunk.track_idx_];

		if (use_offset_map_) chkChunkDetectionAt(chunk, offset);

		if (unknown_length_ && t.is_dummy_) {
			logg(V, "found '", t.codec_.name_, "' chunk inside unknown sequence: ", chunk, "\n");
			unknown_length_ += chunk.size_;
		}
		else if (unknown_length_) {
			noteUnknownSequence(offset);
			logg(V, "found healthy chunk again: ", chunk, "\n");
			correctChunkIdx(chunk.track_idx_);
			disableNoiseBuffer();
		}

		if (last_track_idx_ >= 0) tracks_[last_track_idx_].pushBackLastChunk();
		if (chunk.track_idx_ != idx_free_) chunk_idx_++;

		t.current_chunk_ = chunk;
		t.current_chunk_.alredy_excluded_ = current_mdat_->total_excluded_yet_;

		if (!t.is_dummy_) {
			FrameInfo match(chunk.track_idx_, 0, 0, offset, chunk.sample_size_);
			for (uint n=chunk.n_samples_; n--;) {
				addFrame(match);
				match.offset_ += chunk.sample_size_;
			}
		}

		last_track_idx_ = chunk.track_idx_;
		pkt_idx_ += chunk.n_samples_;
		offset += chunk.size_;

		return true;
	}
	return false;
}

bool Mp4::shouldPreferChunkPrediction() {
	return g_use_chunk_stats &&
	            ((last_track_idx_ >= 0 && tracks_[last_track_idx_].chunkMightBeAtAnd()) ||
	            (last_track_idx_ == -1 && orig_first_track_->shouldUseChunkPrediction())
	            );
}

string Mp4::offToStr(off_t offset) {
	return ss(offset, " / " , current_mdat_->contentStart() + offset);
}

bool Mp4::tryMatch(off_t& offset ) {
	FrameInfo match = getMatch(offset);
	if (match) {
		auto& t = tracks_[match.track_idx_];

		if (use_offset_map_) chkFrameDetectionAt(match, offset);

		if (unknown_length_) {
			noteUnknownSequence(offset);
			logg(V, "found healthy packet again: ", match, "\n");
			correctChunkIdx(match.track_idx_);
			disableNoiseBuffer();
		}
		if (last_track_idx_ != match.track_idx_) {
			if (match.track_idx_ != idx_free_) chunk_idx_++;
			if (last_track_idx_ >= 0) tracks_[last_track_idx_].pushBackLastChunk();
			t.current_chunk_.off_ = offset;
			t.current_chunk_.alredy_excluded_ = current_mdat_->total_excluded_yet_;
		}

		addFrame(match);

		t.current_chunk_.n_samples_++;
		logg(V, t.current_chunk_.n_samples_, "th sample in ", t.chunks_.size()+1, "th ", t.codec_.name_, "-chunk\n");
		last_track_idx_ = match.track_idx_;
		offset += match.length_;
		pkt_idx_++;

		return true;
	}
	return false;
}


void Mp4::repair(const string& filename) {
	if (chkNeedOldApi()) {
		cout << "Help: You need to build against ffmpeg 3.3.\n"
		     << "      See the README.md on how to do that.\n";
		return;
	}
	if (needDynStats()) {
		g_use_chunk_stats = true;
		genDynStats();
		checkForBadTracks();
		if (g_log_mode >= LogMode::V) printDynStats();
		logg(I, "using dynamic stats, use '-is' to see them\n");
	}

	auto& file_read = openFile(filename);

	// TODO: What about multiple mdat?

	logg(V, "calling findMdat on truncated file..\n");
	auto mdat = findMdat(file_read);
	logg(I, "reading mdat from truncated file ...\n");

	if (file_read.length() > (1LL<<32)) {
		broken_is_64_ = true;
		logg(I, "using 64-bit offsets for the broken file\n");
	}

	use_offset_map_ = use_offset_map_ || filename == filename_ok_;
	if (use_offset_map_) analyze(true);

	duration_ = 0;
	for(uint i=0; i < tracks_.size(); i++)
		tracks_[i].clear();

	off_t offset = 0;

	if (g_use_chunk_stats) {
		auto first_off_abs = first_off_abs_ - mdat->contentStart();
		if (first_off_abs > 0 && wouldMatch(first_off_abs)) offset = first_off_abs;
		else if (wouldMatch(first_off_rel_)) offset = first_off_rel_;
		if (offset) {
			logg(V, "beginning at offset ", offToStr(offset), " instead of 0\n");
			addUnknownSequence(0, offset);
		}
	}

	while (chkOffset(offset)) {
		FrameInfo match;
		Mp4::Chunk chunk;

		if (shouldPreferChunkPrediction()) {
			logg(V, "trying chunkPredict first.. \n");
			if (tryChunkPrediction(offset) || tryMatch(offset)) continue;
		}
		else {
			if (tryMatch(offset) || tryChunkPrediction(offset)) continue;
		}

		if (!unknown_length_) {
			if (last_track_idx_ >= 0)
				tracks_[last_track_idx_].pushBackLastChunk();
			last_track_idx_ = idx_free_;  // negative is fine
		}

		if (g_ignore_unknown) {
			if (!g_muted && g_log_mode < LogMode::V) {
				mute();
			}
			else if (!g_noise_buffer_active && g_log_mode >= LogMode::V && !g_dont_omit) {
				logg(V, "unknown sequence -> enabling noise buffer ..\n");
				mute();  // ffmpeg warnings are mostly noise and unhelpful without knowing file offset
			}

			auto step = calcStep(offset);
			unknown_length_ += step;
			offset += step;
		}
		else {
			if (g_muted) unmute();
			double percentage = (double)100 * offset / mdat->contentSize();
			mdat->file_end_ = mdat->contentStart() + offset;
			mdat->length_ = offset + 8;

			logg(E, "unable to find correct codec -> premature end", " (~", setprecision(4), percentage, "%)\n",
			"       try '-s' to skip unknown sequences\n\n");
			logg(V, "mdat->file_end: ", mdat->file_end_, '\n');
			break;
		}
	}

	if (g_muted) unmute();

	for (auto& track : tracks_) track.fixTimes();

	auto filename_fixed = filename + "_fixed" + getOutputSuffix() + getMovExtension(filename_ok_);
	saveVideo(filename_fixed);
}

