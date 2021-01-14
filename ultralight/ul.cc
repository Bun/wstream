#include <thread>
#include <iostream>
#include <functional>

#include <Ultralight/Ultralight.h>
#include <AppCore/Platform.h>

#include "ul.h"


class UltralightBrowser : public ultralight::LoadListener, public ultralight::Logger {
public:
    ultralight::RefPtr<ultralight::Renderer> renderer;
    ultralight::RefPtr<ultralight::View> view;

    virtual void LogMessage(ultralight::LogLevel log_level, const ultralight::String16& message) override;

    virtual void OnFinishLoading(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const ultralight::String& url) override {
        if (is_main_frame) {
            std::cout << "browser: Page has loaded" << std::endl;
        }
    }
};


void ul_browser_copy_bgra(UltralightBrowser* browser, std::function<void(UltralightBrowserDisplay)> f)
{
    ultralight::BitmapSurface* bitmap_surface = (ultralight::BitmapSurface*) browser->view->surface();
    ultralight::RefPtr<ultralight::Bitmap> bitmap = bitmap_surface->bitmap();
    // XXX: sample code suggets we get a BGRA buffer, but it turns out to be
    // RGBA?
    const uint8_t* buf = (const uint8_t*) bitmap->LockPixels();
    UltralightBrowserDisplay ubs = {
        .data = buf,
        .linesize = bitmap->row_bytes(),
    };
    f(ubs);
    bitmap->UnlockPixels();
}


void UltralightBrowser::LogMessage(ultralight::LogLevel log_level, const ultralight::String16& message)
{
     std::cout << ultralight::String(message).utf8().data() << std::endl;
}


void ul_browser_render(UltralightBrowser* browser)
{
    browser->renderer->Update();
    browser->renderer->Render();
}


void ul_browser_url(UltralightBrowser* browser, const char* url)
{
    browser->view->LoadURL(url);
}

UltralightBrowser* ul_browser_new(int width, int height)
{
    UltralightBrowser* ul = new UltralightBrowser();

    ultralight::Config config;
    config.font_family_standard = "Arial";
    config.resource_path = "./resources/";
    config.use_gpu_renderer = false;

    ultralight::Platform::instance().set_config(config);
    ultralight::Platform::instance().set_font_loader(ultralight::GetPlatformFontLoader());
    ultralight::Platform::instance().set_file_system(ultralight::GetPlatformFileSystem("."));
    ultralight::Platform::instance().set_logger(ul);
    ul->renderer = ultralight::Renderer::Create();
    ul->view = ul->renderer->CreateView(width, height, false, nullptr);
    ul->view->set_load_listener(ul);
    return ul;
}
