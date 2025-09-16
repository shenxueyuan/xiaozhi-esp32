#include "emote_display_electron.h"

#include <cstring>
#include <memory>
#include <unordered_map>
#include <tuple>
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <time.h>

#include "display/lcd_display.h"
#include "boards/echoear/mmap_generate_emoji_normal.h"
#include "config.h"
#include "gfx.h"

namespace anim {

static const char* TAG = "electron_emoji";

// UI element management
static gfx_obj_t* obj_label_tips = nullptr;
static gfx_obj_t* obj_label_time = nullptr;
static gfx_obj_t* obj_anim_eye = nullptr;
static gfx_obj_t* obj_anim_mic = nullptr;
static gfx_obj_t* obj_img_icon = nullptr;
static gfx_image_dsc_t icon_img_dsc;

// Track current icon to determine when to show time
static int current_icon_type = MMAP_EMOJI_NORMAL_ICON_BATTERY_BIN;

// ElectronBot specific scaling factor
static const float SCALE_FACTOR = 240.0f / 360.0f; // 0.667
static const int ORIGINAL_EYE_WIDTH = 180;
static const int SCALED_EYE_WIDTH = (int)(ORIGINAL_EYE_WIDTH * SCALE_FACTOR); // 115px

enum class UIDisplayMode : uint8_t {
    SHOW_ANIM_TOP = 1,  // Show obj_anim_mic
    SHOW_TIME = 2,      // Show obj_label_time
    SHOW_TIPS = 3       // Show obj_label_tips
};

static void SetUIDisplayMode(UIDisplayMode mode)
{
    gfx_obj_set_visible(obj_anim_mic, false);
    gfx_obj_set_visible(obj_label_time, false);
    gfx_obj_set_visible(obj_label_tips, false);

    // Show the selected control
    switch (mode) {
    case UIDisplayMode::SHOW_ANIM_TOP:
        gfx_obj_set_visible(obj_anim_mic, true);
        break;
    case UIDisplayMode::SHOW_TIME:
        gfx_obj_set_visible(obj_label_time, true);
        break;
    case UIDisplayMode::SHOW_TIPS:
        gfx_obj_set_visible(obj_label_tips, true);
        break;
    }
}

static void SetupImageDescriptor(mmap_assets_handle_t assets_handle, gfx_image_dsc_t* img_dsc, int asset_id)
{
    const void* img_data = mmap_assets_get_mem(assets_handle, asset_id);
    size_t img_size = mmap_assets_get_size(assets_handle, asset_id);

    // Copy header from image data
    std::memcpy(&img_dsc->header, img_data, sizeof(gfx_image_header_t));
    // Set data pointer to after header
    img_dsc->data = static_cast<const uint8_t*>(img_data) + sizeof(gfx_image_header_t);
    img_dsc->data_size = img_size - sizeof(gfx_image_header_t);
}

static void clock_tm_callback(void* user_data)
{
    // Only display time when battery icon is shown
    if (current_icon_type == MMAP_EMOJI_NORMAL_ICON_BATTERY_BIN) {
        time_t now;
        struct tm timeinfo;
        time(&now);

        setenv("TZ", "GMT+0", 1);
        tzset();
        localtime_r(&now, &timeinfo);

        char time_str[6];
        snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

        gfx_label_set_text(obj_label_time, time_str);
        SetUIDisplayMode(UIDisplayMode::SHOW_TIME);
    }
}

static void InitializeAssets(mmap_assets_handle_t* assets_handle)
{
    const mmap_assets_config_t assets_cfg = {
        .partition_label = "assets_A",
        .max_files = MMAP_EMOJI_NORMAL_FILES,
        .checksum = MMAP_EMOJI_NORMAL_CHECKSUM,
        .flags = {.mmap_enable = true, .full_check = true}
    };

    mmap_assets_new(&assets_cfg, assets_handle);
}

static void InitializeGraphics(esp_lcd_panel_handle_t panel, gfx_handle_t* engine_handle)
{
    gfx_core_config_t gfx_cfg = {
        .flush_cb = ElectronEmoteEngine::OnFlush,
        .user_data = panel,
        .flags = {
            .swap = true,
            .double_buffer = true,
            .buff_dma = true,
        },
        .h_res = DISPLAY_WIDTH,
        .v_res = DISPLAY_HEIGHT,
        .fps = 30,
        .buffers = {
            .buf1 = nullptr,
            .buf2 = nullptr,
            .buf_pixels = DISPLAY_WIDTH * 16,
        },
        .task = GFX_EMOTE_INIT_CONFIG()
    };

    gfx_cfg.task.task_stack_caps = MALLOC_CAP_DEFAULT;
    gfx_cfg.task.task_affinity = 0;
    gfx_cfg.task.task_priority = 5;
    gfx_cfg.task.task_stack = 20 * 1024;

    *engine_handle = gfx_emote_init(&gfx_cfg);
}

static void InitializeEyeAnimation(gfx_handle_t engine_handle, mmap_assets_handle_t assets_handle)
{
    obj_anim_eye = gfx_anim_create(engine_handle);

    const void* anim_data = mmap_assets_get_mem(assets_handle, MMAP_EMOJI_NORMAL_IDLE_ONE_AAF);
    size_t anim_size = mmap_assets_get_size(assets_handle, MMAP_EMOJI_NORMAL_IDLE_ONE_AAF);

    gfx_anim_set_src(obj_anim_eye, anim_data, anim_size);

    // Apply scaling for ElectronBot's 240x240 screen
    int scaled_margin = (int)(10 * SCALE_FACTOR);
    int scaled_y_offset = (int)(-20 * SCALE_FACTOR);

    gfx_obj_align(obj_anim_eye, GFX_ALIGN_LEFT_MID, scaled_margin, scaled_y_offset);

    // Calculate mirror spacing for scaled eyes
    int mirror_spacing = DISPLAY_WIDTH - (SCALED_EYE_WIDTH + scaled_margin) * 2;
    gfx_anim_set_mirror(obj_anim_eye, true, mirror_spacing);

    // Set scale if supported by the graphics engine
    // Note: This may need adjustment based on the actual GFX API
    gfx_anim_set_segment(obj_anim_eye, 0, 0xFFFF, 20, false);
    gfx_anim_start(obj_anim_eye);

    ESP_LOGI(TAG, "Eye animation initialized with scale factor: %.3f, eye width: %d->%d",
             SCALE_FACTOR, ORIGINAL_EYE_WIDTH, SCALED_EYE_WIDTH);
}

static void InitializeFont(gfx_handle_t engine_handle, mmap_assets_handle_t assets_handle)
{
    gfx_font_t font;
    gfx_label_cfg_t font_cfg = {
        .name = "DejaVuSans.ttf",
        .mem = mmap_assets_get_mem(assets_handle, MMAP_EMOJI_NORMAL_KAITI_TTF),
        .mem_size = static_cast<size_t>(mmap_assets_get_size(assets_handle, MMAP_EMOJI_NORMAL_KAITI_TTF)),
    };
    gfx_label_new_font(engine_handle, &font_cfg, &font);

    ESP_LOGI(TAG, "Font initialized, stack: %d", uxTaskGetStackHighWaterMark(nullptr));
}

static void InitializeLabels(gfx_handle_t engine_handle)
{
    // Initialize tips label - scaled for ElectronBot
    obj_label_tips = gfx_label_create(engine_handle);
    gfx_obj_align(obj_label_tips, GFX_ALIGN_TOP_MID, 0, (int)(45 * SCALE_FACTOR));
    gfx_obj_set_size(obj_label_tips, (int)(160 * SCALE_FACTOR), (int)(40 * SCALE_FACTOR));
    gfx_label_set_text(obj_label_tips, "启动中...");
    gfx_label_set_font_size(obj_label_tips, (int)(20 * SCALE_FACTOR));
    gfx_label_set_color(obj_label_tips, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(obj_label_tips, GFX_TEXT_ALIGN_LEFT);
    gfx_label_set_long_mode(obj_label_tips, GFX_LABEL_LONG_SCROLL);
    gfx_label_set_scroll_speed(obj_label_tips, 20);
    gfx_label_set_scroll_loop(obj_label_tips, true);

    // Initialize time label - scaled for ElectronBot
    obj_label_time = gfx_label_create(engine_handle);
    gfx_obj_align(obj_label_time, GFX_ALIGN_TOP_MID, 0, (int)(30 * SCALE_FACTOR));
    gfx_obj_set_size(obj_label_time, (int)(160 * SCALE_FACTOR), (int)(50 * SCALE_FACTOR));
    gfx_label_set_text(obj_label_time, "--:--");
    gfx_label_set_font_size(obj_label_time, (int)(40 * SCALE_FACTOR));
    gfx_label_set_color(obj_label_time, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(obj_label_time, GFX_TEXT_ALIGN_CENTER);
}

static void InitializeMicAnimation(gfx_handle_t engine_handle, mmap_assets_handle_t assets_handle)
{
    obj_anim_mic = gfx_anim_create(engine_handle);
    gfx_obj_align(obj_anim_mic, GFX_ALIGN_TOP_MID, 0, (int)(25 * SCALE_FACTOR));

    const void* anim_data = mmap_assets_get_mem(assets_handle, MMAP_EMOJI_NORMAL_LISTEN_AAF);
    size_t anim_size = mmap_assets_get_size(assets_handle, MMAP_EMOJI_NORMAL_LISTEN_AAF);
    gfx_anim_set_src(obj_anim_mic, anim_data, anim_size);
    gfx_anim_start(obj_anim_mic);
    gfx_obj_set_visible(obj_anim_mic, false);
}

static void InitializeIcon(gfx_handle_t engine_handle, mmap_assets_handle_t assets_handle)
{
    obj_img_icon = gfx_img_create(engine_handle);
    gfx_obj_align(obj_img_icon, GFX_ALIGN_TOP_MID, (int)(-100 * SCALE_FACTOR), (int)(38 * SCALE_FACTOR));

    // Initialize with battery icon first
    SetupImageDescriptor(assets_handle, &icon_img_dsc, MMAP_EMOJI_NORMAL_ICON_BATTERY_BIN);
    gfx_img_set_src(obj_img_icon, static_cast<void*>(&icon_img_dsc));

    ESP_LOGI(TAG, "Icon initialized successfully");
}

static void RegisterCallbacks(esp_lcd_panel_io_handle_t panel_io, gfx_handle_t engine_handle)
{
    esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = ElectronEmoteEngine::OnFlushIoReady,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, engine_handle);

    esp_timer_create_args_t timer_args = {
        .callback = clock_tm_callback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "clock_timer",
        .skip_unhandled_events = true
    };

    esp_timer_handle_t timer_handle;
    esp_timer_create(&timer_args, &timer_handle);
    esp_timer_start_periodic(timer_handle, 10000000); // 10 seconds
}

// ElectronEmoteEngine implementation
ElectronEmoteEngine::ElectronEmoteEngine(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
    : engine_handle_(nullptr), assets_handle_(nullptr)
{
    ESP_LOGI(TAG, "Initializing ElectronBot EmoteEngine with scale factor: %.3f", SCALE_FACTOR);

    InitializeAssets(&assets_handle_);
    InitializeGraphics(panel, &engine_handle_);
    InitializeEyeAnimation(engine_handle_, assets_handle_);
    InitializeFont(engine_handle_, assets_handle_);
    InitializeLabels(engine_handle_);
    InitializeMicAnimation(engine_handle_, assets_handle_);
    InitializeIcon(engine_handle_, assets_handle_);
    RegisterCallbacks(panel_io, engine_handle_);

    ESP_LOGI(TAG, "ElectronBot EmoteEngine initialized successfully");
}

ElectronEmoteEngine::~ElectronEmoteEngine()
{
    if (engine_handle_) {
        gfx_emote_deinit(engine_handle_);
        engine_handle_ = nullptr;
    }

    if (assets_handle_) {
        mmap_assets_del(assets_handle_);
        assets_handle_ = nullptr;
    }
}

void ElectronEmoteEngine::setEyes(int aaf, bool repeat, int fps)
{
    if (!engine_handle_) {
        return;
    }

    const void* src_data = mmap_assets_get_mem(assets_handle_, aaf);
    size_t src_len = mmap_assets_get_size(assets_handle_, aaf);

    Lock();
    gfx_anim_set_src(obj_anim_eye, src_data, src_len);
    gfx_anim_set_segment(obj_anim_eye, 0, 0xFFFF, fps, repeat);
    gfx_anim_start(obj_anim_eye);
    Unlock();
}

void ElectronEmoteEngine::stopEyes()
{
    // Implementation if needed
}

void ElectronEmoteEngine::Lock()
{
    if (engine_handle_) {
        gfx_emote_lock(engine_handle_);
    }
}

void ElectronEmoteEngine::Unlock()
{
    if (engine_handle_) {
        gfx_emote_unlock(engine_handle_);
    }
}

void ElectronEmoteEngine::SetIcon(int asset_id)
{
    if (!engine_handle_) {
        return;
    }

    Lock();
    SetupImageDescriptor(assets_handle_, &icon_img_dsc, asset_id);
    gfx_img_set_src(obj_img_icon, static_cast<void*>(&icon_img_dsc));
    current_icon_type = asset_id;
    Unlock();
}

bool ElectronEmoteEngine::OnFlushIoReady(esp_lcd_panel_io_handle_t panel_io,
                                 esp_lcd_panel_io_event_data_t* edata,
                                 void* user_ctx)
{
    return true;
}

void ElectronEmoteEngine::OnFlush(gfx_handle_t handle, int x_start, int y_start,
                          int x_end, int y_end, const void* color_data)
{
    auto* panel = static_cast<esp_lcd_panel_handle_t>(gfx_emote_get_user_data(handle));
    if (panel) {
        esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, color_data);
    }
    gfx_emote_flush_ready(handle, true);
}

// ElectronEmoteDisplay implementation
ElectronEmoteDisplay::ElectronEmoteDisplay(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    InitializeEngine(panel, panel_io);
}

ElectronEmoteDisplay::~ElectronEmoteDisplay() = default;

void ElectronEmoteDisplay::InitializeEngine(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    emote_engine_ = std::make_unique<ElectronEmoteEngine>(panel, panel_io);
}

void ElectronEmoteDisplay::SetEmotion(const char* emotion)
{
    if (!emote_engine_ || !emotion) {
        return;
    }

    // Emotion mapping from string to AAF asset ID
    static const std::unordered_map<std::string, int> emotion_map = {
        {"neutral", MMAP_EMOJI_NORMAL_IDLE_ONE_AAF},
        {"happy", MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF},
        {"sad", MMAP_EMOJI_NORMAL_SAD_ONE_AAF},
        {"angry", MMAP_EMOJI_NORMAL_ANGRY_ONE_AAF},
        {"surprised", MMAP_EMOJI_NORMAL_SHOCKED_ONE_AAF},
        {"thinking", MMAP_EMOJI_NORMAL_THINKING_ONE_AAF},
        {"confused", MMAP_EMOJI_NORMAL_DIZZY_ONE_AAF},
        {"relaxed", MMAP_EMOJI_NORMAL_ENJOY_ONE_AAF},
        {"listening", MMAP_EMOJI_NORMAL_LISTEN_AAF},
        // Add more mappings as needed
        {"laughing", MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF},
        {"funny", MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF},
        {"loving", MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF},
        {"confident", MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF},
        {"winking", MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF},
        {"cool", MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF},
        {"delicious", MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF},
        {"kissy", MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF},
        {"silly", MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF},
        {"crying", MMAP_EMOJI_NORMAL_SAD_ONE_AAF},
        {"shocked", MMAP_EMOJI_NORMAL_SHOCKED_ONE_AAF},
        {"embarrassed", MMAP_EMOJI_NORMAL_THINKING_ONE_AAF},
        {"sleepy", MMAP_EMOJI_NORMAL_IDLE_ONE_AAF}
    };

    auto it = emotion_map.find(emotion);
    int aaf_id = (it != emotion_map.end()) ? it->second : MMAP_EMOJI_NORMAL_IDLE_ONE_AAF;

    emote_engine_->setEyes(aaf_id, true, 20);
    ESP_LOGI(TAG, "Set emotion: %s (AAF ID: %d)", emotion, aaf_id);
}

void ElectronEmoteDisplay::SetChatMessage(const char* role, const char* content)
{
    if (!content || strlen(content) == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TIME);
        return;
    }

    if (obj_label_tips) {
        gfx_label_set_text(obj_label_tips, content);
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);
    }

    ESP_LOGI(TAG, "Set chat message [%s]: %s", role ? role : "unknown", content);
}

void ElectronEmoteDisplay::ShowListening()
{
    SetUIDisplayMode(UIDisplayMode::SHOW_ANIM_TOP);
}

void ElectronEmoteDisplay::SetIcon(int icon_type)
{
    if (emote_engine_) {
        emote_engine_->SetIcon(icon_type);
    }
}

bool ElectronEmoteDisplay::Lock(int timeout_ms)
{
    if (emote_engine_) {
        emote_engine_->Lock();
        return true;
    }
    return false;
}

void ElectronEmoteDisplay::Unlock()
{
    if (emote_engine_) {
        emote_engine_->Unlock();
    }
}

} // namespace anim
