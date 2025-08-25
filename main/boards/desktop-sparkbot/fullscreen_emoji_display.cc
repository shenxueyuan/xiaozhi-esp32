#include "fullscreen_emoji_display.h"
#include <cstring>
#include <esp_log.h>

static const char* TAG = "FullscreenEmojiDisplay";

// 表情GIF映射表 - 全屏显示
const FullscreenEmojiDisplay::EmotionGifMap FullscreenEmojiDisplay::emotion_gif_maps_[] = {
    // 中性/平静类
    {"neutral", &fullscreen_neutral, 15},
    {"relaxed", &fullscreen_neutral, 10},
    {"sleepy", &fullscreen_neutral, 8},
    
    // 积极/开心类
    {"happy", &fullscreen_happy, 25},
    {"laughing", &fullscreen_happy, 30},
    {"funny", &fullscreen_happy, 30},
    {"loving", &fullscreen_happy, 20},
    {"confident", &fullscreen_happy, 25},
    {"winking", &fullscreen_happy, 15},
    {"cool", &fullscreen_happy, 20},
    {"delicious", &fullscreen_happy, 25},
    {"kissy", &fullscreen_happy, 20},
    {"silly", &fullscreen_happy, 30},
    
    // 悲伤类
    {"sad", &fullscreen_sad, 15},
    {"crying", &fullscreen_sad, 20},
    
    // 愤怒类
    {"angry", &fullscreen_angry, 30},
    
    // 惊讶类
    {"surprised", &fullscreen_surprised, 25},
    {"shocked", &fullscreen_surprised, 30},
    
    // 思考/困惑类
    {"thinking", &fullscreen_thinking, 10},
    {"confused", &fullscreen_thinking, 12},
    {"embarrassed", &fullscreen_thinking, 15},
    
    {nullptr, nullptr, 0}  // 结束标记
};

FullscreenEmojiDisplay::FullscreenEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                             int width, int height, int offset_x, int offset_y,
                                             bool mirror_x, bool mirror_y, bool swap_xy, DisplayFonts fonts)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy, fonts)
    , fullscreen_gif_(nullptr)
    , current_intensity_(2) {
    
    SetupFullscreenGifContainer();
    ESP_LOGI(TAG, "全屏表情显示系统初始化完成 - 尺寸: %dx%d", width, height);
}

FullscreenEmojiDisplay::~FullscreenEmojiDisplay() {
    if (fullscreen_gif_) {
        lv_obj_del(fullscreen_gif_);
    }
}

void FullscreenEmojiDisplay::SetupFullscreenGifContainer() {
    DisplayLockGuard lock(this);
    
    // 创建全屏GIF容器
    fullscreen_gif_ = lv_gif_create(lv_screen_active());
    if (!fullscreen_gif_) {
        ESP_LOGE(TAG, "创建全屏GIF容器失败");
        return;
    }
    
    // 设置为全屏尺寸
    lv_obj_set_size(fullscreen_gif_, width_, height_);
    lv_obj_set_pos(fullscreen_gif_, 0, 0);
    
    // 设置默认表情
    lv_gif_set_src(fullscreen_gif_, &fullscreen_neutral);
    
    // 设置深色主题（适合桌面机器人）
    SetTheme("dark");
    
    ESP_LOGI(TAG, "全屏GIF容器设置完成");
}

void FullscreenEmojiDisplay::SetEmotion(const char* emotion) {
    if (!emotion || !fullscreen_gif_) {
        ESP_LOGW(TAG, "设置表情失败 - emotion: %p, gif: %p", emotion, fullscreen_gif_);
        return;
    }
    
    DisplayLockGuard lock(this);
    
    // 查找匹配的表情GIF
    for (const auto& map : emotion_gif_maps_) {
        if (map.name && strcmp(map.name, emotion) == 0) {
            // 根据强度调整FPS
            int fps = map.default_fps;
            switch (current_intensity_) {
                case 1: fps = fps * 0.7; break;  // 慢速
                case 2: fps = fps; break;        // 正常
                case 3: fps = fps * 1.5; break;  // 快速
                default: fps = fps; break;
            }
            
            PlayGifEmotion(map.gif, fps);
            ESP_LOGI(TAG, "设置全屏表情: %s (FPS: %d, 强度: %d)", emotion, fps, current_intensity_);
            return;
        }
    }
    
    // 未找到匹配表情，使用默认中性表情
    PlayGifEmotion(&fullscreen_neutral, 15);
    ESP_LOGW(TAG, "未知表情 '%s'，使用默认中性表情", emotion);
}

void FullscreenEmojiDisplay::SetStatus(const char* status) {
    if (!status || !fullscreen_gif_) {
        return;
    }
    
    // 可以根据状态显示特定的表情或动画
    if (strcmp(status, "待机") == 0 || strcmp(status, "STANDBY") == 0) {
        SetEmotion("neutral");
    } else if (strcmp(status, "监听中") == 0 || strcmp(status, "LISTENING") == 0) {
        SetEmotion("thinking");
    } else if (strcmp(status, "说话中") == 0 || strcmp(status, "SPEAKING") == 0) {
        SetEmotion("happy");
    }
}

void FullscreenEmojiDisplay::PlayGifEmotion(const lv_image_dsc_t* gif_data, int fps) {
    if (!gif_data || !fullscreen_gif_) {
        return;
    }
    
    // 设置GIF源
    lv_gif_set_src(fullscreen_gif_, gif_data);
    
    // 设置播放速度（通过修改GIF的播放参数）
    // 注意：LVGL的GIF控件可能需要特定的API来控制播放速度
    // 这里假设有相关API，实际使用时需要查看LVGL文档
    
    ESP_LOGD(TAG, "播放全屏GIF表情，FPS: %d", fps);
}

void FullscreenEmojiDisplay::SetEmotionIntensity(int intensity) {
    current_intensity_ = std::max(1, std::min(3, intensity));
    ESP_LOGI(TAG, "设置表情强度: %d", current_intensity_);
}
