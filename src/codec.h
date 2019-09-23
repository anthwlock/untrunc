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
	Codec() = default;
	Codec(AVCodecParameters* c);
	std::string name_;

	void parseOk(Atom* trak);
	bool matchSample(const uchar *start);
	int getSize(const uchar *start, uint maxlength);

	// specific to codec:
	AVCodecParameters* av_codec_params_;
	AVCodecContext* av_codec_context_ = nullptr;
	AvcConfig* avc_config_ = nullptr;

	// info about last frame, codec specific
	bool was_keyframe_ = false;
	bool was_bad_ = false;
	int audio_duration_ = 0;
	bool should_dump_ = false;  // for debug

	bool matchSampleStrict(const uchar* start);
	uint strictness_lvl_ = 0;

	bool isSupported();

private:
	bool (*match_fn_)(Codec*, const uchar* start, int s) = nullptr;
	bool (*match_strict_fn_)(Codec*, const uchar* start, int s) = nullptr;
	int (*get_size_fn_)(Codec*, const uchar* start, uint maxlength) = nullptr;

	void initAVCodec();
};

#endif // CODEC_HPP

