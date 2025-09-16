#pragma once

#include <memory>
#include <esp_lcd_panel_io.h>
#include "display/display.h"
#include "boards/echoear/mmap_generate_emoji_normal.h"
#include "gfx.h"

namespace anim {

/**
 * @brief ElectronBot-specific Emote Engine
 *
 * This class adapts EchoEar's emote engine for ElectronBot's 240x240 display
 * by applying dynamic scaling to fit the smaller screen properly.
 */
class ElectronEmoteEngine {
public:
    ElectronEmoteEngine(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io);
    ~ElectronEmoteEngine();

    void setEyes(int aaf, bool repeat = true, int fps = 20);
    void stopEyes();
    void SetIcon(int asset_id);

    void Lock();
    void Unlock();

    // Static callbacks for graphics engine
    static bool OnFlushIoReady(esp_lcd_panel_io_handle_t panel_io,
                              esp_lcd_panel_io_event_data_t* edata,
                              void* user_ctx);
    static void OnFlush(gfx_handle_t handle, int x_start, int y_start,
                       int x_end, int y_end, const void* color_data);

private:
    gfx_handle_t engine_handle_;
    mmap_assets_handle_t assets_handle_;
};

/**
 * @brief ElectronBot-specific Emote Display
 *
 * This class provides a Display interface implementation that uses
 * EchoEar's emote system with proper scaling for ElectronBot's screen.
 */
class ElectronEmoteDisplay : public Display {
public:
    ElectronEmoteDisplay(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io);
    virtual ~ElectronEmoteDisplay();

    // Display interface implementation
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    // Additional methods for ElectronBot
    void ShowListening();
    void SetIcon(int icon_type);

private:
    void InitializeEngine(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io);

    std::unique_ptr<ElectronEmoteEngine> emote_engine_;
};

} // namespace anim
