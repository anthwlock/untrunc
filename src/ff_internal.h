
extern "C" {
#include <libavcodec/avcodec.h>
}

struct FFCodecDefault;

// https://github.com/FFmpeg/FFmpeg/blob/4243da4ff4/libavcodec/codec_internal.h#L112
// https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/codec_internal.h#L126

typedef struct FFCodec {
    AVCodec p;
    unsigned caps_internal : 29;
    unsigned cb_type : 3;
    int priv_data_size;
    int (*update_thread_context)(struct AVCodecContext *dst,
                                const struct AVCodecContext *src);
    int (*update_thread_context_for_user)(struct AVCodecContext *dst,
                                            const struct AVCodecContext *src);
    const FFCodecDefault *defaults;
    void (*init_static_data)(struct FFCodec *codec);
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
