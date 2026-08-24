
extern "C" {
#include <libavcodec/avcodec.h>
}

struct FFCodecDefault;

// Mirror of libavcodec's *internal* FFCodec, so we can call the decode callback
// directly -- the public API does not tell us how many bytes were consumed.
// https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/codec_internal.h
//
// Only the offset of 'cb' has to match, and it is easy to get wrong silently:
// the call then lands on a different member and the decoder simply reports 0
// consumed bytes, so e.g. mp4a recovery finds nothing at all.
//
// The leading bitfields have been re-split repeatedly (caps_internal, is_decoder,
// color_ranges, alpha_modes, cb_type) but always add up to 32 bits, so they are
// kept opaque here. 'init_static_data' on the other hand was dropped in
// lavc 61.13.100 (ffmpeg 7.1), which does move everything behind it.

typedef struct FFCodec {
    AVCodec p;
    unsigned bitfields_;  // see above, not used by untrunc
    int priv_data_size;
    int (*update_thread_context)(struct AVCodecContext *dst,
                                const struct AVCodecContext *src);
    int (*update_thread_context_for_user)(struct AVCodecContext *dst,
                                            const struct AVCodecContext *src);
    const FFCodecDefault *defaults;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(61, 13, 100)
    void (*init_static_data)(struct FFCodec *codec);
#endif
    int (*init)(struct AVCodecContext *);
    union {
        int (*decode)(struct AVCodecContext *avctx, struct AVFrame *frame,
                    int *got_frame_ptr, struct AVPacket *avpkt);
        int (*decode_sub)(struct AVCodecContext *avctx, struct AVSubtitle *sub,
                        int *got_frame_ptr, struct AVPacket *avpkt);
        int (*receive_frame)(struct AVCodecContext *avctx, struct AVFrame *frame);
        int (*encode)(struct AVCodecContext *avctx, struct AVPacket *avpkt,
                    const struct AVFrame *frame, int *got_packet_ptr);
        int (*encode_sub)(struct AVCodecContext *avctx, uint8_t *buf, int buf_size,
                        const struct AVSubtitle *sub);
        int (*receive_packet)(struct AVCodecContext *avctx, struct AVPacket *avpkt);
    } cb;

    // ..

} FFCodec ;

static av_always_inline const FFCodec *ffcodec(const AVCodec *codec)
{
    return (const FFCodec*)codec;
}
