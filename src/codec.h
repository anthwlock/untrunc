#ifndef CODEC_H
#define CODEC_H

#include <string>
#include <vector>

#include "common.h"

class AVCodecContext;
class AVCodecParameters;

class Atom;
class AvcConfig;

class Codec {
public:
	Codec() = default;
	Codec(AVCodecParameters* c);
	static void initOnce();
	std::string name_;

	void parseOk(Atom* trak);
	bool matchSample(const uchar *start);
	int getSize(const uchar *start, uint maxlength, off_t offset);

	// specific to codec:
	AVCodecParameters* av_codec_params_;
	AVCodecContext* av_codec_context_ = nullptr;
	AvcConfig* avc_config_ = nullptr;

	// info about last frame, codec specific
	bool was_keyframe_ = false;
	bool was_bad_ = false;
	int audio_duration_ = 0;
	bool should_dump_ = false;  // for debug
	bool chk_for_twos_ = false;

	bool matchSampleStrict(const uchar* start);
	uint strictness_lvl_ = 0;
	off_t cur_off_ = 0;

	bool isSupported();
	const uchar* loadAfter(off_t offset);

	static bool looksLikeTwosOrSowt(const uchar* start);
	static bool twos_is_sowt;  // used in looksLikeTwosOrSowt

private:
	bool (*match_fn_)(Codec*, const uchar* start, uint s) = nullptr;
	bool (*match_strict_fn_)(Codec*, const uchar* start, uint s) = nullptr;
	int (*get_size_fn_)(Codec*, const uchar* start, uint maxlength) = nullptr;

	void initAVCodec();
};

#endif // CODEC_H

