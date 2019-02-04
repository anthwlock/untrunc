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
	void parse(Atom *trak, std::vector<int> &offsets, Atom *mdat);
	bool matchSample(const uchar *start) const;
	int getSize(const uchar *start, uint maxlength, int &duration, bool &is_bad);

	// specific to codec:
	AVCodecContext *context_;
	AVCodec *codec_;
	AvcConfig* avc_config_;

	bool was_keyframe;
	void parse(Atom* trak, const std::vector<uint>& offsets, const std::vector<int64_t>& offsets_64, Atom* mdat, bool is64);
	bool matchSampleStrict(const uchar* start) const;
private:
	int n_channels_ = 0;
	AVPacket* packet_;
	AVFrame* frame_;

//	struct BufEntry{int size, nb_samples;};
//	std::queue<BufEntry> buffer_;
};

#endif // CODEC_HPP
