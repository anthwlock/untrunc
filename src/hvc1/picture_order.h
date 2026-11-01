#ifndef HEVC_PICTURE_ORDER_H
#define HEVC_PICTURE_ORDER_H

#include <memory>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "../common.h"

class BufferedAtom;
class Track;

struct HevcPicture {
	int poc;
	bool keyframe, resets_poc;
};

class HevcPictureOrder {
public:
	explicit HevcPictureOrder(const AVCodecParameters* parameters);
	HevcPicture parse(BufferedAtom& mdat, off_t off, uint32_t size);
	void apply(Track& track);

private:
	static void freeContext(AVCodecContext* context);
	std::unique_ptr<AVCodecContext, decltype(&freeContext)> context_{nullptr, freeContext};
	std::unique_ptr<AVCodecParserContext, decltype(&av_parser_close)> parser_{nullptr, av_parser_close};
	std::vector<std::pair<int, size_t>> group_; // (picture order count, decode index)
	std::vector<size_t> presentation_order_;
	size_t n_pictures_ = 0;
	void finishGroup();
};

void recoverHevcCompositionOffsets(Track& track, BufferedAtom& mdat);

#endif // HEVC_PICTURE_ORDER_H
