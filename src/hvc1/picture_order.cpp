#include "picture_order.h"
#include "nal.h"
#include "../mp4.h"

extern "C" {
#include <libavutil/mem.h>
}

using namespace std;

void HevcPictureOrder::freeContext(AVCodecContext* context) {
	avcodec_free_context(&context);
}

HevcPictureOrder::HevcPictureOrder(const AVCodecParameters* parameters) {
	context_.reset(avcodec_alloc_context3(nullptr));
	parser_.reset(av_parser_init(AV_CODEC_ID_HEVC));
	if (!context_ || !parser_)
		throw string("HEVC picture order requires the FFmpeg HEVC parser (--enable-parser=hevc)");
	if (avcodec_parameters_to_context(context_.get(), parameters) < 0)
		throw string("Could not initialize HEVC parameters");
	parser_->flags |= PARSER_FLAG_COMPLETE_FRAMES;

	// FFmpeg 3.x's parser expects Annex B, including its parameter sets.
	auto p = parameters->extradata;
	int length = parameters->extradata_size;
	if (length < 23 || p[0] != 1 || (p[21] & 3) != 3)
		throw string("Unsupported HEVC configuration");
	vector<uint8_t> annexb;
	int off = 23;
	for (int array = 0; array < p[22]; ++array) {
		if (length - off < 3) throw string("Incomplete HEVC configuration");
		int count = (p[off + 1] << 8) | p[off + 2];
		off += 3;
		for (int i = 0; i < count; ++i) {
			if (length - off < 2) throw string("Incomplete HEVC configuration");
			int size = (p[off] << 8) | p[off + 1];
			off += 2;
			if (size > length - off) throw string("Incomplete HEVC configuration");
			annexb.insert(annexb.end(), {0, 0, 0, 1});
			annexb.insert(annexb.end(), p + off, p + off + size);
			off += size;
		}
	}
	av_freep(&context_->extradata);
	context_->extradata_size = annexb.size();
	context_->extradata = static_cast<uint8_t*>(av_mallocz(annexb.size() + AV_INPUT_BUFFER_PADDING_SIZE));
	if (!context_->extradata) throw string("Could not allocate HEVC configuration");
	memcpy(context_->extradata, annexb.data(), annexb.size());
}

HevcPicture HevcPictureOrder::parse(BufferedAtom& mdat, off_t off, uint32_t size) {
	vector<uint8_t> annexb;
	while (size >= 6) {
		H265NalInfo nal(mdat.getFragment(off, 6), size);
		if (!nal.is_ok || nal.length_ < 6 || nal.nuh_layer_id_ != 0) break;
		if (nal.nal_type_ <= 31 || (nal.nal_type_ >= 32 && nal.nal_type_ <= 34)) {
			// Retain in-band parameter sets (hev1). For pictures, only the first
			// slice header is needed; do not read or decode compressed pixels.
			int count = nal.nal_type_ <= 31 ? min<uint32_t>(nal.length_ - 4, 64) : nal.length_ - 4;
			if (count > 1 << 20) break;
			annexb.insert(annexb.end(), {0, 0, 0, 1});
			auto data = mdat.getFragment(off + 4, count);
			annexb.insert(annexb.end(), data, data + count);
		}
		if (nal.nal_type_ <= 31) {
			int length = annexb.size();
			annexb.resize(length + AV_INPUT_BUFFER_PADDING_SIZE);
			uint8_t* out;
			int out_size;
			parser_->output_picture_number = numeric_limits<int>::min();
			av_parser_parse2(parser_.get(), context_.get(), &out, &out_size,
			                 annexb.data(), length, AV_NOPTS_VALUE, AV_NOPTS_VALUE, -1);
			if (parser_->output_picture_number == numeric_limits<int>::min()) break;
			HevcPicture picture{parser_->output_picture_number, h265IsKeyframe(nal.nal_type_),
			                    nal.nal_type_ >= 16 && nal.nal_type_ <= 20};
			if (picture.resets_poc) finishGroup();
			group_.emplace_back(picture.poc, n_pictures_++);
			return picture;
		}
		off += nal.length_;
		size -= nal.length_;
	}
	throw ss("Could not parse HEVC picture at ", g_mp4->offToStr(off));
}

void HevcPictureOrder::finishGroup() {
	sort(group_.begin(), group_.end());
	for (size_t i = 0; i < group_.size(); ++i) {
		if (i && group_[i - 1].first == group_[i].first)
			throw string("Duplicate HEVC picture order counts");
		presentation_order_.push_back(group_[i].second);
	}
	group_.clear();
}

void HevcPictureOrder::apply(Track& track) {
	finishGroup();
	if (presentation_order_.size() != track.getNumSamples())
		throw string("Incomplete HEVC picture order");
	if (presentation_order_.empty()) return;
	vector<int64_t> presentation_times(presentation_order_.size());
	int64_t time = 0;
	// Assign pictures to the existing time grid in display order. Keep the
	// duration sequence intact, including fractional-frame rounding ticks.
	for (size_t i = 0; i < presentation_order_.size(); ++i) {
		presentation_times[presentation_order_[i]] = time;
		time += track.getTime(i);
	}
	vector<int> offsets;
	time = 0;
	for (size_t i = 0; i < presentation_times.size(); ++i) {
		auto offset = presentation_times[i] - time;
		if (offset < numeric_limits<int>::min() || offset > numeric_limits<int>::max())
			throw string("HEVC composition offset is too large");
		offsets.push_back(offset);
		time += track.getTime(i);
	}
	track.composition_offsets_ = move(offsets);
	if (g_no_ctts) return;

	// Keep leading pictures at time zero even when the first picture in
	// decode order has a later presentation timestamp.
	auto edts = new Atom;
	edts->name_ = "edts";
	auto elst = new Atom;
	elst->name_ = "elst";
	auto duration = track.getDurationInTimescale();
	bool wide = duration > numeric_limits<uint32_t>::max();
	elst->content_.resize(wide ? 28 : 20);
	elst->writeInt(wide ? 0x01000000 : 0, 0);
	elst->writeInt(1, 4);
	if (wide) {
		elst->writeInt64(duration, 8);
		elst->writeInt64(0, 16);
	}
	else {
		elst->writeInt(duration, 8);
		elst->writeInt(0, 12);
	}
	elst->writeInt(0x00010000, wide ? 24 : 16);
	edts->children_.push_back(elst);
	track.trak_->children_.push_back(edts);
}

void recoverHevcCompositionOffsets(Track& track, BufferedAtom& mdat) {
	if (g_no_ctts || !contains({"hvc1", "hev1"}, track.codec_.name_) || !track.getNumSamples()) return;
	// Allow a single rounding tick for constant rates that do not divide the
	// timescale. Picture order alone cannot recover genuinely variable timing.
	int shortest = track.getTime(0), longest = shortest;
	for (size_t i = 0; i < track.getNumSamples(); ++i) {
		shortest = min(shortest, track.getTime(i));
		longest = max(longest, track.getTime(i));
	}
	if (shortest <= 0 || longest - shortest > 1) {
		logg(V, "keeping reference composition offsets for variable-duration HEVC\n");
		return;
	}
	try {
		HevcPictureOrder pictures(track.codec_.av_codec_params_);
		size_t sample = 0;
		for (auto& chunk : track.chunks_) {
			auto offset = chunk.off_;
			for (int i = 0; i < chunk.n_samples_; ++i) {
				if (sample >= track.getNumSamples()) throw string("HEVC chunk/sample count mismatch");
				int size = track.getSize(sample);
				if (size < 7 || offset < 0 || size > mdat.contentSize() - offset)
					throw string("Incomplete HEVC sample");
				pictures.parse(mdat, offset, size);
				offset += track.getSizeWithGcd(sample++);
			}
		}
		pictures.apply(track);
		logg(I, "reconstructed HEVC composition offsets from picture order (", sample, " frames)\n");
	}
	catch (const string& error) {
		logg(W, error, "; keeping reference composition offsets\n");
	}
}
