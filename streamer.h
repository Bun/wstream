#pragma once

typedef struct StreamConfig {
    char *rtmpServer;
    int width, height;
    int fps;
    int bitrate;
} StreamConfig;

typedef struct Streamer Streamer;

Streamer *streamer_new(StreamConfig);
int streamer_send_frame(Streamer *);
int streamer_close(Streamer *);
int streamer_connect(Streamer *);
void streamer_delay_next(Streamer *);

typedef struct StreamerFrame {
    uint8_t* const* data;
    const int* linesize;
} StreamerFrame;

StreamerFrame streamer_frame(Streamer *s);
void streamer_swap_frame(Streamer *s);
