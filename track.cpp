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
#include <cstring>
#include <iostream>
#include <iomanip>
#include <limits>
#include <string>
#include <utility>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}  // extern "C"
#include "atom.h"
#include "common.h"


using std::cout;
using std::move;
using std::pair;
using std::string;
using std::vector;


Track::Track(Atom* trak, AVCodecContext* context)
	: trak_(trak), codec_(context)
{ }

void Track::cleanUp() {
	timescale_ = 0;
	duration_  = 0;
	n_matched_ = 0;
	file_offsets_.clear();
	sizes_.clear();
	keyframes_.clear();
	times_.clear();
	//codec_.clear();
}

bool Track::parse(Atom* mdat) {
	cleanUp();

	if (!trak_) {
		logg(E, "Track not initialized.\n");
		return false;
	}

	Atom* mdhd = trak_->find1stAtom("mdhd");
	if (!mdhd) {
		logg(E, "Missing atom: ", Atom::completeName("mdhd"),
			    " -> Unknown duration and timescale.\n");
		throw string("Missing atom mdhd -> Unknown duration and timescale");
		return false;
	}

	timescale_ = mdhd->readUint32(12);
	duration_  = mdhd->readUint32(16);

	times_     = getSampleTimes(trak_);
	keyframes_ = getKeyframes(trak_);
	sizes_     = getSampleSizes(trak_);

	auto chunk_offsets   = getChunkOffsets(trak_);
	auto sample_to_chunk = getSampleToChunk(trak_, chunk_offsets.size());

	if (times_.size() != sizes_.size()) {
		logg(W, "Mismatch between time offsets and size offsets: \n"
			    "Time offsets: ", times_.size(),
				" Size offsets: ", sizes_.size(), '\n');
	}
	//assert(times_.size() == sizes_.size());
	if (times_.size() != sample_to_chunk.size()) {
		logg(W, "Mismatch between time offsets and sample_to_chunk offsets: \n"
			    "Time offsets: ", times_.size(),
			    " Chunk offsets: ", sample_to_chunk.size(), '\n');
	}

	// Compute actual file offsets.
	uint32_t prev_chunk  = -1;
	uint64_t file_offset =  0;
	for (unsigned int i = 0; i < sizes_.size(); ++i) {
		if (i < sample_to_chunk.size()) {
			auto chunk = sample_to_chunk[i];
			if (chunk != prev_chunk || i == 0) {
				prev_chunk  = chunk;
				file_offset = chunk_offsets[chunk];
			}
		}
		file_offsets_.push_back(file_offset);
		file_offset += sizes_[i];
	}

	// Move this stuff into track!
	Atom* hdlr = trak_->find1stAtom("hdlr");
	if (!hdlr) {
		logg(E, "Missing atom: ", Atom::completeName("hdlr"), ".\n");
		return false;
	}
	uint32_t hdlr_type = hdlr->readUint32(8);
	if (hdlr_type != name2Id("soun") && hdlr_type != name2Id("vide")) {
		logg(V, "Not an audio nor video track.\n");
		return true;
	}

	// Move this to Codec.
	if (!codec_.parse(trak_, file_offsets_, mdat)) return false;
	// If audio, use next?

	// Print sizes and offsets.
	logg(V, "Track codec: ", codec_.name_, '\n');
	logg(V, "Sizes      : ", sizes_.size(), '\n');
	for (unsigned int i = 0; i < 10 && i < sizes_.size(); ++i) {
		if (!mdat || mdat->contentPos() < 0 ||
			file_offsets_[i] < static_cast<unsigned decltype(
				mdat->contentPos())>(mdat->contentPos())) {
			logg(V, std::setw(8), i,
			        " Size: ", std::setw(6), sizes_[i],
			        " offset ", std::setw(10), file_offsets_[i],
			        " < ", std::setw(10), mdat->contentPos(), '\n');
			continue;
		}
		int64_t  offset = file_offsets_[i] - mdat->contentPos();
		uint32_t begin  = mdat->readUint32(offset);
		uint32_t next   = mdat->readUint32(offset + 4);
		uint32_t end    = mdat->readUint32(offset + sizes_[i] - 4);
		logg(V, std::setw(8), i,
			    " Size: ", std::setw(6), sizes_[i],
			    " offset ", std::setw(10), file_offsets_[i],
			    "  begin: ", std::hex, std::setw(5), begin,
				' ', std::setw(8), next,
			    " end: ", std::setw(8), end, std::dec, '\n');
	}
	if (sizes_.size() > 10)
		logg(V, "       ... (", sizes_.size() - 10, " more)\n");
	logg(V);

	return true;
}

void Track::writeToAtoms() {
	if (!trak_) return;

	if (keyframes_.empty())
		trak_->pruneAtoms("stss");

	saveSampleTimes();
	saveKeyframes();
	saveSampleSizes();
	saveSampleToChunk();
	saveChunkOffsets();

	Atom* mdhd = trak_->find1stAtom("mdhd");
	if (!mdhd)
		logg(W, "Missing atom: ", Atom::completeName("mdhd"), ".\n");
	else
		mdhd->writeUint32(16, duration_);

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
	const uint32_t pps[15] = {
		0x28ee3880, 0x28ee1620, 0x28ee1e20, 0x28ee0988, 0x28ee0b88,
		0x28ee0d88, 0x28ee0f88, 0x28ee0462, 0x28ee04e2, 0x28ee0562,
		0x28ee05e2, 0x28ee0662, 0x28ee06e2, 0x28ee0762, 0x28ee07e2};
	if (codec_.name_ == "avc1") {
		Atom* stsd = trak->find1stAtom("stsd");
		if (stsd) {
			// A bit complicated to find... find avcC... follow first link.
			stsd->writeUint32(122, pps[SHANE]);
		}
	}
#endif
}

void Track::clear() {
	file_offsets_.clear();
	sizes_.clear();
	keyframes_.clear();
	//times_.clear();
}

void Track::fixTimes() {
	if (codec_.name_ == "samr") {
		times_.clear();
		times_.resize(file_offsets_.size(), 160);
		return;
	}
	//for(int i = 0; i != 100; i++)
	//	cout << "times_[" << i << "] = " << times_[i] << '\n';
	while (times_.size() < file_offsets_.size())
		times_.insert(times_.end(), times_.begin(), times_.end());
	times_.resize(file_offsets_.size());

	duration_ = 0;
	for (const auto& time : times_)
		duration_ += time;
}


vector<uint32_t> Track::getSampleTimes(Atom* trak) {
	assert(trak);
	vector<uint32_t> sample_times;

	Atom* stts = trak->find1stAtom("stts");
	if (!stts) {
		// Atom stts is mandatory.
		logg(E, "Missing atom: ", Atom::completeName("stts"), ".\n");
		throw string("Missing atom: ") + Atom::completeName("stts");
		return sample_times;
	}

	uint32_t entries = stts->readUint32(4);
	for (unsigned int i = 0; i < entries; ++i) {
		uint32_t sample_count = stts->readUint32( 8 + 8 * i);
		uint32_t sample_delta = stts->readUint32(12 + 8 * i);
		sample_times.insert(sample_times.end(), sample_count, sample_delta);
	}
	return sample_times;
}

vector<uint32_t> Track::getKeyframes(Atom* trak) {
	assert(trak);
	vector<uint32_t> sample_keys;

	Atom* stss = trak->find1stAtom("stss");
	if (!stss) {
		// Atom stss is not mandatory.
		logg(V, "Missing optional atom: ", Atom::completeName("stss"), ".\n");
		return sample_keys;
	}

	uint32_t entries = stss->readUint32(4);
	for (unsigned int i = 0; i < entries; ++i) {
		uint32_t sample_number = stss->readUint32(8 + 4 * i);
		// MP4 indexes are 1-based and Untrunc indexes are 0-based.
		if (sample_number <= 0) {
			logg(W, "Invalid key frame (", sample_number, ") in atom: ",
				    stss->completeName(), ".\n");
		} else {
			sample_number--;
		}
		sample_keys.push_back(sample_number);
	}
	return sample_keys;
}

vector<uint32_t> Track::getSampleSizes(Atom* trak) {
	assert(trak);
	vector<uint32_t> sample_sizes;

	Atom* stsz = trak->find1stAtom("stsz");
	Atom* stz2 = trak->find1stAtom("stz2");
	if (!stsz && !stz2) {
		// Either atom stsz or atom stz2 is mandatory.
		logg(E, "Missing atoms: ", Atom::completeName("stsz"), " & ",
			    Atom::completeName("stz2"), ".\n");
		throw string("Missing atoms: ") + Atom::completeName("stsz") +
			  " & " + Atom::completeName("stz2");
		return sample_sizes;
	}
	if (stsz && stz2)
		logg(W, "Found both atoms stsz & stz2 -> using atom stsz.\n");

	if (stsz) {
		uint32_t sample_size = stsz->readUint32(4);
		uint32_t entries     = stsz->readUint32(8);
		if (sample_size == 0) {  // Different sizes.
			for (unsigned int i = 0; i < entries; ++i)
				sample_sizes.push_back(stsz->readUint32(12 + 4 * i));
		} else {
			sample_sizes.resize(entries, sample_size);
		}
	} else {  // stz2.
		uint8_t  field_size = stz2->readUint8(7);
		uint32_t entries    = stz2->readUint32(8);
		switch (field_size) {
			case  4: 
				for (unsigned int i = 0; i < (entries + 1) / 2; ++i) {
					uint8_t entry_size = stz2->readUint8(12 + i);
					sample_sizes.push_back(entry_size >> 4);
					sample_sizes.push_back(entry_size & 0xF);
				}
				sample_sizes.resize(entries);
				break;
			case  8: 
				for (unsigned int i = 0; i < entries; ++i)
					sample_sizes.push_back(stz2->readUint8( 12 + 1 * i));
				break;
			case 16: 
				for (unsigned int i = 0; i < entries; ++i)
					sample_sizes.push_back(stz2->readUint16(12 + 2 * i));
				break;
			case 24:  // Unofficial; not in MP4.
				for (unsigned int i = 0; i < entries; ++i)
					sample_sizes.push_back(stz2->readUint24(12 + 3 * i));
				break;
			case 32:  // Unofficial; not in MP4.
				for (unsigned int i = 0; i < entries; ++i)
					sample_sizes.push_back(stz2->readUint32(12 + 4 * i));
				break;
			default:
				logg(E, "Unsupported field size (", field_size, ") in atom: ",
					    stz2->completeName(), ".\n");
				throw string("Unsupported field size in atom: ") +
					  stz2->completeName();
				break;
		}
	}
	return sample_sizes;
}


vector<uint64_t> Track::getChunkOffsets(Atom* trak) {
	assert(trak);
	vector<uint64_t> chunk_offsets;

	Atom* stco = trak->find1stAtom("stco");
	Atom* co64 = trak->find1stAtom("co64");
	if (!stco && !co64) {
		// Either atom stco or atom co64 is mandatory.
		logg(E, "Missing atoms: ", Atom::completeName("stco"), " & ",
			    Atom::completeName("co64"), ".\n");
		throw string("Missing atoms: ") + Atom::completeName("stco") +
			  " & " + Atom::completeName("co64");
		return chunk_offsets;
	}
	if (stco && co64)
		logg(W, "Found both atoms stco & co64 -> using atom stco.\n");

	if (stco) {
		uint32_t entries = stco->readUint32(4);
		for (unsigned int i = 0; i < entries; ++i)
			chunk_offsets.push_back(stco->readUint32(8 + i * 4));
	} else {  // co64.
		uint32_t entries = co64->readUint32(4);
		for (unsigned int i = 0; i < entries; ++i)
			chunk_offsets.push_back(co64->readUint64(8 + i * 8));
	}
	return chunk_offsets;
}

#ifdef TRACK_SAMPLES_TO_DESCR
pair<vector<uint32_t>,vector<uint32_t>> Track::getSampleToChunk(
											Atom* trak, uint32_t num_chunks) {
	assert(trak);
	vector<uint32_t> samples_to_chunk;
	vector<uint32_t> samples_to_descr;

	Atom* stsc = trak->find1stAtom("stsc");
	if (!stsc) {
		// Atom stsc is mandatory.
		logg(E, "Missing atom: ", Atom::completeName("stsc"), ".\n");
		throw string("Missing atom: ") + Atom::completeName("stsc");
		return std::make_pair(samples_to_chunk, samples_to_descr);
	}

	samples_to_chunk.reserve(num_chunks);
	samples_to_descr.reserve(num_chunks);
	if (++num_chunks == 0) num_chunks = std::numeric_limits<uint32_t>::max();
	uint32_t entries = stsc->readUint32(4);
	for (unsigned int i = 1; i <= entries; ++i) {
		uint32_t first_chunk       = stsc->readUint32( 8 + 12 * i - 12);
		uint32_t samples_per_chunk = stsc->readUint32(12 + 12 * i - 12);
		uint32_t sample_descr_idx  = stsc->readUint32(16 + 12 * i - 12);
		uint32_t last_chunk        = (i == entries) ? num_chunks
									 : stsc->readUint32(8 + 12 * i);

		if (first_chunk >= last_chunk) {
			logg(E, "Incorrect sample chunk index order (", first_chunk, " >= ",
				    last_chunk, ") in atom: ", stsc->completeName(), ".\n");
			continue;
		}

		for (unsigned int chunk = first_chunk; chunk < last_chunk; ++chunk) {
			// MP4 indexes are 1-based and Untrunc indexes are 0-based.
			uint32_t chunk_index = chunk - 1;
			if (chunk <= 0) {
				logg(W, "Invalid sample chunk index (", chunk, ") in atom: ",
						stsc->completeName(), ".\n");
				chunk_index = 0;
			}
			samples_to_chunk.insert(samples_to_chunk.end(), samples_per_chunk,
									chunk_index);
		}

		// MP4 indexes are 1-based and Untrunc indexes are 0-based.
		if (sample_descr_idx  <= 0) {
			logg(W, "Invalid sample description index (", sample_descr_idx,
					") in atom: ", stsc->completeName(), ".\n");
		} else {
			sample_descr_idx--;
		}
		size_t samples = (last_chunk - first_chunk);
		samples *= samples_per_chunk;
		samples_to_descr.insert(samples_to_descr.end(), samples,
								sample_descr_idx);
	}
	return std::make_pair(samples_to_chunk, samples_to_descr);
}
#else
vector<uint32_t> Track::getSampleToChunk(Atom* trak, uint32_t num_chunks) {
	assert(trak);
	vector<uint32_t> samples_to_chunk;

	Atom* stsc = trak->find1stAtom("stsc");
	if (!stsc) {
		// Atom stsc is mandatory.
		logg(E, "Missing atom: ", Atom::completeName("stsc"), ".\n");
		throw string("Missing atom: ") + Atom::completeName("stsc");
		return samples_to_chunk;
	}

	samples_to_chunk.reserve(num_chunks);
	if (++num_chunks == 0) num_chunks = std::numeric_limits<uint32_t>::max();
	uint32_t entries = stsc->readUint32(4);
	for (unsigned int i = 0; i < entries; ++i) {
		uint32_t first_chunk       = stsc->readUint32( 8 + 12 * i);
		uint32_t samples_per_chunk = stsc->readUint32(12 + 12 * i);
		uint32_t last_chunk        = (i + 1 == entries) ? num_chunks
									 : stsc->readUint32(8 + 12 + 12 * i);

		if (first_chunk >= last_chunk) {
			logg(E, "Incorrect sample chunk index order (", first_chunk, " >= ",
				    last_chunk, ") in atom: ", stsc->completeName(), ".\n");
			continue;
		}

		for (unsigned int chunk = first_chunk; chunk < last_chunk; ++chunk) {
			// MP4 indexes are 1-based and Untrunc indexes are 0-based.
			uint32_t chunk_index = chunk - 1;
			if (chunk <= 0) {
				logg(W, "Invalid sample chunk index (", chunk, ") in atom: ",
						stsc->completeName(), ".\n");
				chunk_index = 0;
			}
			samples_to_chunk.insert(samples_to_chunk.end(), samples_per_chunk,
									chunk_index);
		}
	}
	return samples_to_chunk;
}
#endif


void Track::saveSampleTimes() {
	if (!trak_) return;

	Atom* stts = trak_->find1stAtom("stts");
	if (!stts) {
		Atom* stbl = trak_->find1stAtom("stbl");
		if (stbl) {
			stts = new Atom("stts");
			stbl->addChild(move(stts));
			stts = stbl->findChild("stts");
		}
	}
	assert(stts);  // Atom stts is mandatory.
	if (!stts) {
		logg(E, "Cannot save atom: ", Atom::completeName("stts"));
		return;
	}

	vector<pair<uint32_t, uint32_t>> vp;
	for (const auto& sample_delta : times_) {
		if (vp.empty() || sample_delta != vp.back().second)
			vp.emplace_back(1, sample_delta);
		else
			vp.back().first++;
	}
	if (vp.size() > std::numeric_limits<uint32_t>::max()) {
		logg(W, "Atom stts table entries exceeds limit (", vp.size(),
			    " -> ", std::numeric_limits<uint32_t>::max(), ").\n");
		vp.resize(std::numeric_limits<uint32_t>::max());
	}

	stts->contentResize(1 + 3 +          // Version + Flags +
						4 +              // Number of entries +
						8 * vp.size());  // Time table.
	stts->writeUint32(4, vp.size());
	unsigned int cnt = 0;
	for (const auto& p : vp) {
		stts->writeUint32( 8 + 8 * cnt, p.first);   // sample_count.
		stts->writeUint32(12 + 8 * cnt, p.second);  // sample_delta (time).
		cnt++;
	}
}

void Track::saveKeyframes() {
	if (!trak_) return;
	if (keyframes_.empty()) {
		trak_->pruneAtoms("stss");
		return;
	}

	Atom* stss = trak_->find1stAtom("stss");
	if (!stss) {
		Atom* stbl = trak_->find1stAtom("stbl");
		if (stbl) {
			stss = new Atom("stss");
			stbl->addChild(move(stss));
			stss = stbl->findChild("stss");
		}
	}
	if (!stss) {
		logg(W, "Cannot save atom: ", Atom::completeName("stss"));
		return;
	}

	uint32_t entries = keyframes_.size();
	if (keyframes_.size() > std::numeric_limits<uint32_t>::max()) {
		logg(W, "Atom stss table entries exceeds limit (", keyframes_.size(),
			    " -> ", std::numeric_limits<uint32_t>::max(), ").\n");
		entries = std::numeric_limits<uint32_t>::max();
	}

	stss->contentResize(1 + 3 +        // Version + Flags +
						4 +            // Number of entries +
						4 * entries);  // Time table.
	stss->writeUint32(4, entries);
	for (unsigned int i = 0; i < entries; ++i) {
		// MP4 indexes are 1-based and Untrunc indexes are 0-based.
		uint32_t sample_number = (keyframes_[i] + 1);
		if (keyframes_[i] >= std::numeric_limits<uint32_t>::max()) {
			logg(W, "Invalid key frame for atom: ",
				    stss->completeName(), ".\n");
			sample_number = keyframes_[i];
		}
		stss->writeUint32(8 + 4 * i, sample_number);
	}
}

void Track::saveSampleSizes() {
	if (!trak_) return;

	trak_->pruneAtoms("stz2");
	Atom* stsz = trak_->find1stAtom("stsz");
	if (!stsz) {
		Atom* stbl = trak_->find1stAtom("stbl");
		if (stbl) {
			stsz = new Atom("stsz");
			stbl->addChild(move(stsz));
			stsz = stbl->findChild("stsz");
		}
	}
	assert(stsz);  // Either atom stsz or atom stz2 is mandatory.
	if (!stsz) {
		logg(E, "Cannot save atom: ", Atom::completeName("stsz"));
		return;
	}

	uint32_t entries = sizes_.size();
	if (sizes_.size() > std::numeric_limits<uint32_t>::max()) {
		logg(W, "Atom stsz table entries exceeds limit (", sizes_.size(),
			    " -> ", std::numeric_limits<uint32_t>::max(), ").\n");
		entries = std::numeric_limits<uint32_t>::max();
	}
	uint32_t sample_size = 0;
	bool     same_sizes  = true;
	if (entries > 0) {
		sample_size = sizes_[0];
		for (const auto& sz : sizes_) {
			if (sz == sample_size) continue;
			same_sizes = false;
			break;
		}
	}

	if (same_sizes) {
		stsz->contentResize(1 + 3 +    // Version + Flags +
							4 +        // Default size +
							4);        // Number of entries.
		if (entries > 0) stsz->writeUint32(4, sample_size);
		stsz->writeUint32(8, entries);
		return;
	}
	stsz->contentResize(1 + 3 +        // Version + Flags +
						4 +            // Default size +
						4 +            // Number of entries +
						4 * entries);  // Size table.
	stsz->writeUint32(4, 0);
	stsz->writeUint32(8, entries);
	for (unsigned int i = 0; i < sizes_.size(); ++i)
		stsz->writeUint32(12 + 4 * i, sizes_[i]);
}

void Track::saveSampleToChunk() {
	if (!trak_) return;

	Atom* stsc = trak_->find1stAtom("stsc");
	if (!stsc) {
		Atom* stbl = trak_->find1stAtom("stbl");
		if (stbl) {
			stsc = new Atom("stsc");
			stbl->addChild(move(stsc));
			stsc = stbl->findChild("stsc");
		}
	}
	assert(stsc);  // Atom stsc is mandatory.
	if (!stsc) {
		logg(E, "Cannot save atom: ", Atom::completeName("stsc"));
		return;
	}

	stsc->contentResize(1 + 3 +  // Version + Flags +
						4 +      // Number of entries +
						12 * 1); // Sample to chunk table.
	stsc->writeUint32( 4, 1);  // 1 table entry.
	stsc->writeUint32( 8, 1);  // First chunk (1-based).
	stsc->writeUint32(12, 1);  // Samples per chunk.
	stsc->writeUint32(16, 1);  // Index into the sample descr. stsd (1-based).
}

void Track::saveChunkOffsets() {
	if (!trak_) return;

	bool useCo64 = false;
	for (const auto& file_offset : file_offsets_) {
		// MP4 stco offsets are uint32_t, but some implemenations use int32_t.
		// QuickTime/Mov uses co64 if the high bit is set.
		if (file_offset <= std::numeric_limits<int32_t>::max()) continue;
		useCo64 = true;
		break;
	}
	uint32_t entries = file_offsets_.size();
	if (file_offsets_.size() > std::numeric_limits<uint32_t>::max()) {
		logg(W, "Atom ", ((useCo64) ? "co64" : "stco"),
			    " table entries exceeds limit (", file_offsets_.size(),
			    " -> ", std::numeric_limits<uint32_t>::max(), ").\n");
		entries = std::numeric_limits<uint32_t>::max();
	}

	trak_->pruneAtoms((useCo64) ? name2Id("stco") : name2Id("co64"));
	uint32_t co_id   = (useCo64) ? name2Id("co64") : name2Id("stco");
	Atom*    co_atom = trak_->find1stAtom(co_id);
	if (!co_atom) {
		Atom* stbl = trak_->find1stAtom("stbl");
		if (stbl) {
			co_atom = new Atom(co_id);
			stbl->addChild(move(co_atom));
			co_atom = stbl->findChild(co_id);
		}
	}
	assert(co_atom);  // Either atom co64 or atom co64 is mandatory.
	if (!co_atom) {
		logg(E, "Cannot save atom: ", Atom::completeName(co_id));
		return;
	}

	if (useCo64) {
		co_atom->contentResize(1 + 3 +        // Version + Flags +
							   4 +            // Number of entries +
							   8 * entries);  // Chunk offset table.
		co_atom->writeUint32(4, entries);
		for (unsigned int i = 0; i < entries; ++i)
			co_atom->writeUint64(8 + 8 * i, file_offsets_[i]);
	} else {
		co_atom->contentResize(1 + 3 +        // Version + Flags +
							   4 +            // Number of entries +
							   4 * entries);  // Chunk offset table.
		co_atom->writeUint32(4, entries);
		for (unsigned int i = 0; i < entries; ++i)
			co_atom->writeUint32(8 + 4 * i, file_offsets_[i]);
	}
}


// vim:set ts=4 sw=4 sts=4 noet:
