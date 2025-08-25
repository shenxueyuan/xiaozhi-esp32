#pragma once

#include "display/lcd_display.h"
#include <libs/gif/lv_gif.h>
#include <unordered_map>
#include <string>

// 复用Otto Robot的GIF表情资源
#include <otto_emoji_gif.h>

class FullscreenEmojiDisplay : public SpiLcdDisplay {
public:
    FullscreenEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                          int width, int height, int offset_x, int offset_y,
                          bool mirror_x, bool mirror_y, bool swap_xy, DisplayFonts fonts);

    virtual ~FullscreenEmojiDisplay();

    // 重写表情设置方法，实现全屏GIF显示
    virtual void SetEmotion(const char* emotion) override;

    // 重写状态设置方法
    virtual void SetStatus(const char* status) override;

    // 设置表情强度（影响动画速度）
    void SetEmotionIntensity(int intensity);

private:
    void SetupFullscreenGifContainer();
    void PlayGifEmotion(const lv_image_dsc_t* gif_data, int fps = 25);

    lv_obj_t* fullscreen_gif_;
    int current_intensity_;

    // 表情映射表
    struct EmotionGifMap {
        const char* name;
        const lv_image_dsc_t* gif;
        int default_fps;
    };
    static const EmotionGifMap emotion_gif_maps_[];
};
