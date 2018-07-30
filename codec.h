#ifndef CODEC_HPP
#define CODEC_HPP

#include <string>
#include <vector>

#include "common.h"

class Atom;
class AVCodecContext;
class AVCodec;

class AvcConfig;
class AudioConfig;

class Codec {
public:
	Codec(AVCodecContext* c);
	std::string name_;
	void parse(Atom *trak, std::vector<int> &offsets, Atom *mdat);
	bool matchSample(const uchar *start);
	int getLength(const uchar *start, uint maxlength, int &duration);
	//used by: mp4a
	int mask1_;
	int mask0_;
	AVCodecContext *context_;
	AVCodec *codec_;

	AvcConfig* avc_config_;
	AudioConfig* audio_config_;

	bool last_frame_was_idr_;
};

#endif // CODEC_HPP
