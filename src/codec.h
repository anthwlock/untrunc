#ifndef CODEC_HPP
#define CODEC_HPP

#include <string>
#include <vector>

#include "common.h"

class Atom;
class AVCodecContext;
class AVCodecParameters;
class AVCodec;
class AVPacket;
class AVFrame;
class AvcConfig;
class AVCodecParserContext;

class Codec {
public:
	Codec(AVCodecParameters* c);
	std::string name_;

	void parse(Atom* trak, const std::vector<uint64_t>& offsets, Atom* mdat);
	bool matchSample(const uchar *start) const;
	int getSize(const uchar *start, uint maxlength);

	// specific to codec:
	AVCodecParameters* av_codec_params_;
	AVCodec* av_codec_;
	AVCodecContext* av_codec_context_ = nullptr;
	AvcConfig* avc_config_ = nullptr;

	// info about last frame, codec specific
	bool was_keyframe_ = false;
	bool was_bad_ = false;
	int audio_duration_ = 0;

	bool matchSampleStrict(const uchar* start) const;
	uint strictness_level_ = 0;

private:
	int n_channels_ = 0;
	uint fdsc_idx_ = -1;  // GoPro specific
//	int tmcd_seen_ = 0;  // GoPro specific
	AVPacket* packet_;
	AVFrame* frame_;
};

#endif // CODEC_HPP
