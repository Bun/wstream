#pragma once

typedef struct UltralightBrowserDisplay {
    const uint8_t* data;
    uint32_t linesize;
} UltralightBrowserDisplay;

class UltralightBrowser;

UltralightBrowser *ul_browser_new(int width, int height);
void ul_browser_close(UltralightBrowser*);
void ul_browser_render(UltralightBrowser*);
void ul_browser_copy_bgra(UltralightBrowser*, std::function<void(UltralightBrowserDisplay)>);
void ul_browser_url(UltralightBrowser* browser, const char* url);
