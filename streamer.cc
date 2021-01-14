#include <mutex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
}

#include "streamer.h"

#define DEFAULT_PIX_FMT AV_PIX_FMT_YUV420P

typedef struct Streamer {
    StreamConfig config;

    AVRational time_base;
    AVFormatContext *container;
    AVStream *video;
    AVCodecContext *video_enc;
    AVPacket *pkt;

    std::mutex doubleBufferLock;
    AVFrame *frame;
    AVFrame *drawTo;

    int pts;
    int64_t time;
    int64_t target;
} Streamer;


// streamer_frame returns a frame that can be safely written to by a different
// thread. The buffer remains valid until the next call to streamer_swap_frame.
StreamerFrame streamer_frame(Streamer *s)
{
    StreamerFrame sf;
    av_frame_make_writable(s->drawTo);
    sf.data = (uint8_t * const*) s->drawTo->data;
    sf.linesize = (const int *) s->drawTo->linesize;
    return sf;
}

void streamer_swap_frame(Streamer *s)
{
    std::unique_lock<std::mutex> lock(s->doubleBufferLock);
    AVFrame *cur = s->frame;
    s->frame = s->drawTo;
    s->drawTo = cur;
    // TODO: instead of blocking, set "wants to swap" flag so when drawing is
    // finished, the frame can be swapped
}


static int streamer_cleanup(Streamer *s)
{
    if (s->frame != NULL) {
        av_frame_free(&s->frame);
        s->frame = NULL;
    }

    if (s->drawTo != NULL) {
        av_frame_free(&s->drawTo);
        s->drawTo = NULL;
    }

    if (s->pkt != NULL) {
        av_packet_free(&s->pkt);
        s->pkt = NULL;
    }

    if (s->video_enc != NULL) {
        avcodec_free_context(&s->video_enc);
        s->video_enc = NULL;
    }

    if (s->container != NULL) {
        if (!(s->container->oformat->flags & AVFMT_NOFILE)) {
            avio_close(s->container->pb); // TODO: report error code
        }
        avformat_free_context(s->container);
        s->container = NULL;
    }

    return 0;
}

// streamer_rtmp_init opens the connection to the RMTP server.
static int streamer_rtmp_init(Streamer *s)
{
    if (!(s->container->oformat->flags & AVFMT_NOFILE)) {
        int ret = avio_open(&s->container->pb, s->config.rtmpServer, AVIO_FLAG_WRITE);
        if (ret < 0)
            return ret;
    }

    int ret = avformat_write_header(s->container, NULL);
    if (ret < 0)
        return ret;

    s->time = av_gettime_relative();
    return 0;
}

// streamer_encoder_init initializes a H.264 encoder in a FLV container.
static int streamer_encoder_init(Streamer *s)
{
    AVCodec *codec;
    int ret;

    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
        return -1;

    s->pkt = av_packet_alloc();

    s->frame = av_frame_alloc();
    if (!s->frame) return -1;
    s->frame->format = AV_PIX_FMT_YUV420P;
    s->frame->width = s->config.width;
    s->frame->height = s->config.height;
    if ((ret = av_frame_get_buffer(s->frame, 0)) < 0) return ret;

    s->drawTo = av_frame_alloc();
    if (!s->drawTo) return -1;
    s->drawTo->format = AV_PIX_FMT_YUV420P;
    s->drawTo->width = s->config.width;
    s->drawTo->height = s->config.height;
    if ((ret = av_frame_get_buffer(s->drawTo, 0)) < 0) return ret;

    avformat_alloc_output_context2(&s->container, NULL, "flv", NULL);
    if (!s->container) {
        return -1;
    }

    //
    //
    //
    AVCodecContext *c = avcodec_alloc_context3(codec);

    c->codec_tag = 0;
    c->width = s->config.width;
    c->height = s->config.height;
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->framerate = av_inv_q(s->time_base);
    c->time_base = s->time_base;
    c->max_b_frames = 5;
    c->qmin = 0;
    c->qmax = 69;
    c->gop_size = 15;
    c->bit_rate = (s->config.bitrate-500)*1000;
    c->rc_max_rate = s->config.bitrate*1000;
    c->rc_buffer_size = s->config.bitrate*1000;

    if (s->container->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_opt_set(c->priv_data, "tune", "zerolatency", 0);
    av_opt_set(c->priv_data, "preset", "veryfast", 0);
    //av_opt_set(c->priv_data, "live", "1", 0);
    av_opt_set_int(c->priv_data, "keyint", 30, 0);
    av_opt_set_int(c->priv_data, "keyint_min", 3, 0);

    if ((ret = avcodec_open2(c, codec, NULL)) < 0)
        return ret;

    AVStream *video = avformat_new_stream(s->container, NULL);
    s->video = video;
    avcodec_parameters_from_context(video->codecpar, c);

    s->video_enc = c;

    return 0;
}

// TODO: adjust delay based on observed time-to-encode
int streamer_send_frame(Streamer *s)
{
    std::unique_lock<std::mutex> lock(s->doubleBufferLock);
    int ret;
    AVFrame *frame = s->frame;

    frame->pts = s->pts;
    s->pts++;
    if ((ret = avcodec_send_frame(s->video_enc, frame)) < 0)
        return ret;

    for (;;) {
        ret = avcodec_receive_packet(s->video_enc, s->pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0)
            return ret;

        av_packet_rescale_ts(s->pkt, s->time_base, s->video->time_base);
        ret = av_interleaved_write_frame(s->container, s->pkt);
        av_packet_unref(s->pkt);
        if (ret < 0)
            return ret;
    }
    return 0;
}

void streamer_delay_next(Streamer *s)
{
    int64_t now = av_gettime_relative();
    int64_t next = s->target + s->time;
    int64_t delta = next - now;
    if (delta > 0) {
        if (0) printf("delay: %zd\n", delta);
        av_usleep(delta);
    } else {
        printf("slow frame last=%ld now=%ld delta=%ld\n", s->time, now, delta);
    }
    s->time = next;
}

int streamer_connect(Streamer *s)
{
    int ret;
    streamer_cleanup(s);

    s->target = (1000 * 1000) / s->config.fps;
    s->time_base = (AVRational) {1, s->config.fps};
    s->pts = 1;

    if ((ret = streamer_encoder_init(s)) < 0)
        return ret;
    return streamer_rtmp_init(s);
}

int streamer_close(Streamer *s)
{
    if (s) {
        //av_write_trailer(ofmt_ctx);
        streamer_cleanup(s);
        free(s);
    }
    return 0;
}

Streamer *streamer_new(StreamConfig cfg)
{
    Streamer *s = (Streamer *) calloc(1, sizeof(Streamer));
    if (!s) return NULL;
    s->config = cfg;
    return s;
}
