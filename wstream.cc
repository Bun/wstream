#include <iostream>
#include <thread>
#include <mutex>
#include <functional>

extern "C" {
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <libswscale/swscale.h>
}

#include "streamer.h"
#include "ultralight/ul.h"


typedef struct Display {
    Streamer *s;
    struct SwsContext *ctx;
    int width, height;
    std::mutex frameProtect;
} Display;


static void ul_browser_thread(Display* d)
{
    auto browser = ul_browser_new(d->width, d->height);
    ul_browser_url(browser, "https://das.awoo.nl/twitch/stream/");

    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(7));
        ul_browser_render(browser);
        auto sf = streamer_frame(d->s);
        {
            std::unique_lock<std::mutex> lock(d->frameProtect);
            ul_browser_copy_bgra(browser, [&](UltralightBrowserDisplay ubs) {
                int linesize = ubs.linesize;
                sws_scale(d->ctx, &ubs.data, &linesize, 0,
                    d->height, sf.data, sf.linesize);
                });
        }
        streamer_swap_frame(d->s); // XXX: blocks while encoding
    }
}

int main()
{
    StreamConfig cfg = {
        .rtmpServer = getenv("RTMP_URL"),
        .width = 1280,
        .height = 720,
        .fps = 25,
        .bitrate = 5000,
    };

    if (cfg.rtmpServer == NULL || cfg.rtmpServer[0] == 0) {
        fprintf(stderr, "RTMP_URL is required");
        return 1;
    }

    Streamer *s = streamer_new(cfg);
    if (!s) abort();

    // TODO: configuration for source resolution and pixel format
    Display *d = new Display();
    d->width = cfg.width;
    d->height = cfg.height;
    d->s = s;
    d->ctx = sws_getContext(
        cfg.width, cfg.height, AV_PIX_FMT_RGB32,
        cfg.width, cfg.height, AV_PIX_FMT_YUV420P,
        SWS_BICUBIC, NULL, NULL, NULL);

    std::thread bt(ul_browser_thread, d);

    for (;;) {
        printf("stream: Connecting...\n");
        // TODO: auto sleep to prevent rapid reconnect

        if (streamer_connect(s) < 0) {
            printf("streamer_connect failed\n");
            break;
        }

        for (int i = 0; ; i = (i + 1) & 0xff) {
            if (i == 0) printf(".\n");
            int ret = streamer_send_frame(s);
            if (ret < 0) {
                fprintf(stderr, "streamer_write_frame: %d\n", ret);
                break;
            }
            streamer_delay_next(s);
        }

        // <cleanup>
    }

    streamer_close(s);
    sws_freeContext(d->ctx);
    bt.join(); // FIXME
}
