#include "emote_display.h"

#include <cstring>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <tuple>
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <time.h>

#include "display/lcd_display.h"
#include "mmap_generate_emoji_normal.h"
#if defined(CONFIG_BOARD_TYPE_ELECTRON_BOT)
#include "boards/electron-bot/config.h"
#else
#include "config.h"
#endif
#include "gfx.h"
// Plan1: 使用 AAF 解码私有头进行自渲染缩放
#include "decoder/gfx_aaf_format.h"
#include "decoder/gfx_aaf_dec.h"
#include "decoder/gfx_jpeg_dec.h"
#include <esp_heap_caps.h>
// Plan1 自渲染缩放所需解码接口
#include "decoder/gfx_aaf_format.h"
#include "decoder/gfx_aaf_dec.h"

namespace anim {

static const char* TAG = "emoji";

// UI element management
static gfx_obj_t* obj_label_tips = nullptr;
static gfx_obj_t* obj_label_time = nullptr;
static gfx_obj_t* obj_anim_eye_left = nullptr;
static gfx_obj_t* obj_anim_eye_right = nullptr;
static gfx_obj_t* obj_anim_mic = nullptr;
static gfx_obj_t* obj_img_icon = nullptr;
static gfx_image_dsc_t icon_img_dsc;

// 方案1全局缩放帧（供 OnFlush 合成，避免与 GFX 刷新打架导致闪烁）
static uint16_t* g_eye_frame = nullptr; // 持久 DMA 缓冲，存放单眼缩放后的 RGB565
static int g_eye_w = 0;
static int g_eye_h = 0;
static int g_eye_left_x = 0;
static int g_eye_left_y = 0;
static int g_eye_right_x = 0;
static int g_eye_right_y = 0;

// Track current icon to determine when to show time
static int current_icon_type = MMAP_EMOJI_NORMAL_ICON_BATTERY_BIN;

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
        .flush_cb = EmoteEngine::OnFlush,
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
    obj_anim_eye_left = gfx_anim_create(engine_handle);
    obj_anim_eye_right = gfx_anim_create(engine_handle);

    const void* anim_data = mmap_assets_get_mem(assets_handle, MMAP_EMOJI_NORMAL_IDLE_ONE_AAF);
    size_t anim_size = mmap_assets_get_size(assets_handle, MMAP_EMOJI_NORMAL_IDLE_ONE_AAF);

    gfx_anim_set_src(obj_anim_eye_left, anim_data, anim_size);
    gfx_anim_set_src(obj_anim_eye_right, anim_data, anim_size);

    // 240x240：左右各放一只眼睛；方案1自渲染，因此隐藏GFX动画对象，避免重叠
    const int gap_between = 10;
    const int target_eye_w = 100;
    const int target_eye_h = 100;
    int total_w = target_eye_w * 2 + gap_between;
    int left_x = std::max(0, (DISPLAY_WIDTH - total_w) / 2);
    int right_x = std::min((int)DISPLAY_WIDTH - target_eye_w, left_x + target_eye_w + gap_between);
    int center_y = std::max(0, (DISPLAY_HEIGHT - target_eye_h) / 2);
    gfx_obj_align(obj_anim_eye_left, GFX_ALIGN_TOP_LEFT, left_x, center_y);
    gfx_obj_align(obj_anim_eye_right, GFX_ALIGN_TOP_LEFT, right_x, center_y);
    gfx_obj_set_visible(obj_anim_eye_left, false);
    gfx_obj_set_visible(obj_anim_eye_right, false);
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

    ESP_LOGI(TAG, "stack: %d", uxTaskGetStackHighWaterMark(nullptr));
}

static void InitializeLabels(gfx_handle_t engine_handle)
{
    // Initialize tips label
    obj_label_tips = gfx_label_create(engine_handle);
    gfx_obj_align(obj_label_tips, GFX_ALIGN_TOP_MID, 0, 50);
    gfx_obj_set_size(obj_label_tips, 140, 36);
    gfx_label_set_text(obj_label_tips, "启动中...");
    gfx_label_set_font_size(obj_label_tips, 20);
    gfx_label_set_color(obj_label_tips, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(obj_label_tips, GFX_TEXT_ALIGN_LEFT);
    gfx_label_set_long_mode(obj_label_tips, GFX_LABEL_LONG_SCROLL);
    gfx_label_set_scroll_speed(obj_label_tips, 20);
    gfx_label_set_scroll_loop(obj_label_tips, true);

    // Initialize time label
    obj_label_time = gfx_label_create(engine_handle);
    gfx_obj_align(obj_label_time, GFX_ALIGN_TOP_MID, 0, 32);
    gfx_obj_set_size(obj_label_time, 140, 46);
    gfx_label_set_text(obj_label_time, "--:--");
    gfx_label_set_font_size(obj_label_time, 40);
    gfx_label_set_color(obj_label_time, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(obj_label_time, GFX_TEXT_ALIGN_CENTER);
}

static void InitializeMicAnimation(gfx_handle_t engine_handle, mmap_assets_handle_t assets_handle)
{
    obj_anim_mic = gfx_anim_create(engine_handle);
    gfx_obj_align(obj_anim_mic, GFX_ALIGN_TOP_MID, 0, 25);

    const void* anim_data = mmap_assets_get_mem(assets_handle, MMAP_EMOJI_NORMAL_LISTEN_AAF);
    size_t anim_size = mmap_assets_get_size(assets_handle, MMAP_EMOJI_NORMAL_LISTEN_AAF);
    gfx_anim_set_src(obj_anim_mic, anim_data, anim_size);
    gfx_anim_start(obj_anim_mic);
    gfx_obj_set_visible(obj_anim_mic, false);
}

static void InitializeIcon(gfx_handle_t engine_handle, mmap_assets_handle_t assets_handle)
{
    obj_img_icon = gfx_img_create(engine_handle);
    gfx_obj_align(obj_img_icon, GFX_ALIGN_TOP_MID, -80, 38);

    SetupImageDescriptor(assets_handle, &icon_img_dsc, MMAP_EMOJI_NORMAL_ICON_WIFI_FAILED_BIN);
    gfx_img_set_src(obj_img_icon, static_cast<void*>(&icon_img_dsc));
}

static void RegisterCallbacks(esp_lcd_panel_io_handle_t panel_io, gfx_handle_t engine_handle)
{
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = EmoteEngine::OnFlushIoReady,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, engine_handle);
}

void SetupImageDescriptor(mmap_assets_handle_t assets_handle,
                          gfx_image_dsc_t* img_dsc,
                          int asset_id)
{
    const void* img_data = mmap_assets_get_mem(assets_handle, asset_id);
    size_t img_size = mmap_assets_get_size(assets_handle, asset_id);

    std::memcpy(&img_dsc->header, img_data, sizeof(gfx_image_header_t));
    img_dsc->data = static_cast<const uint8_t*>(img_data) + sizeof(gfx_image_header_t);
    img_dsc->data_size = img_size - sizeof(gfx_image_header_t);
}

EmoteEngine::EmoteEngine(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    ESP_LOGI(TAG, "Create EmoteEngine, panel: %p, panel_io: %p", panel, panel_io);

    InitializeAssets(&assets_handle_);
    InitializeGraphics(panel, &engine_handle_);

    // 保存面板句柄供方案1直接绘制
    panel_ = panel;
    panel_ = panel;

    gfx_emote_lock(engine_handle_);
    gfx_emote_set_bg_color(engine_handle_, GFX_COLOR_HEX(0x000000));

    // Initialize all UI components
    InitializeEyeAnimation(engine_handle_, assets_handle_);
    InitializeFont(engine_handle_, assets_handle_);
    InitializeLabels(engine_handle_);
    InitializeMicAnimation(engine_handle_, assets_handle_);
    InitializeIcon(engine_handle_, assets_handle_);

    current_icon_type = MMAP_EMOJI_NORMAL_ICON_WIFI_FAILED_BIN;
    SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);

    gfx_timer_create(engine_handle_, clock_tm_callback, 1000, obj_label_tips);

    gfx_emote_unlock(engine_handle_);

    RegisterCallbacks(panel_io, engine_handle_);
}

EmoteEngine::~EmoteEngine()
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

void EmoteEngine::setEyes(int aaf, bool repeat, int fps)
{
    if (!engine_handle_) {
        return;
    }

    const void* src_data = mmap_assets_get_mem(assets_handle_, aaf);
    size_t src_len = mmap_assets_get_size(assets_handle_, aaf);

    Lock();
    gfx_anim_set_src(obj_anim_eye_left, src_data, src_len);
    gfx_anim_set_src(obj_anim_eye_right, src_data, src_len);
    // 切换动画源后，维持目标尺寸并重新布局
    const int target_eye_w = 60;
    const int target_eye_h = 60;
    const int gap_between = 10;
    gfx_obj_set_size(obj_anim_eye_left, target_eye_w, target_eye_h);
    gfx_obj_set_size(obj_anim_eye_right, target_eye_w, target_eye_h);
    int total_w = target_eye_w * 2 + gap_between;
    int left_x = std::max(0, (DISPLAY_WIDTH - total_w) / 2);
    int right_x = std::min((int)DISPLAY_WIDTH - target_eye_w, left_x + target_eye_w + gap_between);
    int center_y = std::max(0, (DISPLAY_HEIGHT - target_eye_h) / 2);
    gfx_obj_align(obj_anim_eye_left, GFX_ALIGN_TOP_LEFT, left_x, center_y);
    gfx_obj_align(obj_anim_eye_right, GFX_ALIGN_TOP_LEFT, right_x, center_y);
    gfx_anim_set_segment(obj_anim_eye_left, 0, 0xFFFF, fps, repeat);
    gfx_anim_set_segment(obj_anim_eye_right, 0, 0xFFFF, fps, repeat);
    gfx_anim_start(obj_anim_eye_left);
    gfx_anim_start(obj_anim_eye_right);
    Unlock();
}

void EmoteEngine::stopEyes()
{
    // Implementation if needed
}

void EmoteEngine::Lock()
{
    if (engine_handle_) {
        gfx_emote_lock(engine_handle_);
    }
}

void EmoteEngine::Unlock()
{
    if (engine_handle_) {
        gfx_emote_unlock(engine_handle_);
    }
}

void EmoteEngine::SetIcon(int asset_id)
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

bool EmoteEngine::OnFlushIoReady(esp_lcd_panel_io_handle_t panel_io,
                                 esp_lcd_panel_io_event_data_t* edata,
                                 void* user_ctx)
{
    return true;
}

void EmoteEngine::OnFlush(gfx_handle_t handle, int x_start, int y_start,
                          int x_end, int y_end, const void* color_data)
{
    auto* panel = static_cast<esp_lcd_panel_handle_t>(gfx_emote_get_user_data(handle));
    if (panel) {
        // 先将 GFX 缓冲刷到屏幕
        esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, color_data);

        // 再叠加绘制两只眼的缩放帧（若已准备好），避免闪烁
        if (g_eye_frame && g_eye_w > 0 && g_eye_h > 0) {
            // 计算与当前 flush 区域的交集，尽量只在可见时绘制
            int fw = x_end - x_start;
            int fh = y_end - y_start;
            // 左眼：使用原坐标（整页由面板做180°旋转）
            int lx1 = g_eye_left_x, ly1 = g_eye_left_y;
            int lx2 = lx1 + g_eye_w, ly2 = ly1 + g_eye_h;
            if (!(lx2 <= x_start || lx1 >= x_end || ly2 <= y_start || ly1 >= y_end)) {
                esp_lcd_panel_draw_bitmap(panel, lx1, ly1, lx2, ly2, g_eye_frame);
            }
            // 右眼：使用原坐标
            int rx1 = g_eye_right_x, ry1 = g_eye_right_y;
            int rx2 = rx1 + g_eye_w, ry2 = ry1 + g_eye_h;
            if (!(rx2 <= x_start || rx1 >= x_end || ry2 <= y_start || ry1 >= y_end)) {
                esp_lcd_panel_draw_bitmap(panel, rx1, ry1, rx2, ry2, g_eye_frame);
            }
        }
    }
    gfx_emote_flush_ready(handle, true);
}

// EmoteDisplay implementation
EmoteDisplay::EmoteDisplay(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    InitializeEngine(panel, panel_io);
    // 启动方案1自渲染任务
    if (scaled_task_ == nullptr) {
        xTaskCreate([](void* arg){ static_cast<EmoteDisplay*>(arg)->ScaledLoop(); }, "scaled_emote", 4096, this, 4, &scaled_task_);
    }
}

EmoteDisplay::~EmoteDisplay() = default;

void EmoteDisplay::SetEmotion(const char* emotion)
{
    if (!engine_) {
        return;
    }

    using EmotionParam = std::tuple<int, bool, int>;
    static const std::unordered_map<std::string, EmotionParam> emotion_map = {
        {"happy",       {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"laughing",    {MMAP_EMOJI_NORMAL_ENJOY_ONE_AAF,     true,  20}},
        {"funny",       {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"loving",      {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"embarrassed", {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"confident",   {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"delicious",   {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"sad",         {MMAP_EMOJI_NORMAL_SAD_ONE_AAF,       true,  20}},
        {"crying",      {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"sleepy",      {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"silly",       {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"angry",       {MMAP_EMOJI_NORMAL_ANGRY_ONE_AAF,     true,  20}},
        {"surprised",   {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"shocked",     {MMAP_EMOJI_NORMAL_SHOCKED_ONE_AAF,   true,  20}},
        {"thinking",    {MMAP_EMOJI_NORMAL_THINKING_ONE_AAF,  true,  20}},
        {"winking",     {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"relaxed",     {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
        {"confused",    {MMAP_EMOJI_NORMAL_DIZZY_ONE_AAF,     true,  20}},
        {"neutral",     {MMAP_EMOJI_NORMAL_IDLE_ONE_AAF,      false, 20}},
        {"idle",        {MMAP_EMOJI_NORMAL_IDLE_ONE_AAF,      false, 20}},
    };

    auto it = emotion_map.find(emotion);
    if (it != emotion_map.end()) {
        current_asset_id_ = std::get<0>(it->second);
        current_repeat_   = std::get<1>(it->second);
        current_fps_      = std::get<2>(it->second);
    } else {
        current_asset_id_ = MMAP_EMOJI_NORMAL_IDLE_ONE_AAF;
        current_repeat_   = false;
        current_fps_      = 20;
    }
}

void EmoteDisplay::SetChatMessage(const char* role, const char* content)
{
    engine_->Lock();
    if (content && strlen(content) > 0) {
        gfx_label_set_text(obj_label_tips, content);
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);
    }
    engine_->Unlock();
}

void EmoteDisplay::SetStatus(const char* status)
{
    if (!engine_) {
        return;
    }

    if (std::strcmp(status, "聆听中...") == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_ANIM_TOP);
        engine_->setEyes(MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF, true, 20);
        engine_->SetIcon(MMAP_EMOJI_NORMAL_ICON_MIC_BIN);
    } else if (std::strcmp(status, "待命") == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TIME);
        engine_->SetIcon(MMAP_EMOJI_NORMAL_ICON_BATTERY_BIN);
    } else if (std::strcmp(status, "说话中...") == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);
        engine_->SetIcon(MMAP_EMOJI_NORMAL_ICON_SPEAKER_ZZZ_BIN);
    } else if (std::strcmp(status, "错误") == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);
        engine_->SetIcon(MMAP_EMOJI_NORMAL_ICON_WIFI_FAILED_BIN);
    }

    engine_->Lock();
    if (std::strcmp(status, "连接中...") != 0) {
        gfx_label_set_text(obj_label_tips, status);
    }
    engine_->Unlock();
}

void EmoteDisplay::InitializeEngine(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    engine_ = std::make_unique<EmoteEngine>(panel, panel_io);
}

bool EmoteDisplay::Lock(int timeout_ms)
{
    return true;
}

void EmoteDisplay::Unlock()
{
    // Implementation if needed
}

// 方案1：解码-缩放-绘制循环
void EmoteDisplay::ScaledLoop()
{
    const int target_eye_w = 100;
    const int target_eye_h = 100;
    const int gap_between = 10;
    int last_asset = -1;
    gfx_aaf_format_handle_t fmt = nullptr;

    while (true) {
        int asset_id = current_asset_id_;
        if (asset_id != last_asset) {
            if (fmt) { gfx_aaf_format_deinit(fmt); fmt = nullptr; }
            auto assets = engine_->GetAssetsHandle();
            const uint8_t* data = (const uint8_t*)mmap_assets_get_mem(assets, asset_id);
            size_t size = mmap_assets_get_size(assets, asset_id);
            // ESP_LOGI(TAG, "scaled:init asset=%d data=%p size=%u", asset_id, data, (unsigned)size);

            if (data && size > 0) {
                gfx_aaf_format_init(data, size, &fmt);
                // ESP_LOGI(TAG, "scaled:init fmt=%p", fmt);
            }
            last_asset = asset_id;
        }

        int total_w = target_eye_w * 2 + gap_between;
        int left_x = std::max(0, (DISPLAY_WIDTH - total_w) / 2);
        int right_x = std::min((int)DISPLAY_WIDTH - target_eye_w, left_x + target_eye_w + gap_between);
        int center_y = std::max(0, (DISPLAY_HEIGHT - target_eye_h) / 2);

        if (fmt == nullptr) {
            // ESP_LOGW(TAG, "scaled: fmt==nullptr, wait");
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        int total_frames = gfx_aaf_format_get_total_frames(fmt);
        if (total_frames <= 0) total_frames = 1;
        // ESP_LOGI(TAG, "scaled: frames=%d fps=%d pos L(%d,%d) R(%d,%d) size=%dx%d", total_frames, (int)current_fps_, left_x, center_y, right_x, center_y, target_eye_w, target_eye_h);

        for (int fi = 0; fi < total_frames; ++fi) {
            // 如果切换了素材，退出到外层重新初始化
            if (current_asset_id_ != asset_id) break;
            const uint8_t* frame_data = gfx_aaf_format_get_frame_data(fmt, fi);
            int frame_size = gfx_aaf_format_get_frame_size(fmt, fi);
            if (!frame_data || frame_size <= 0) continue;

            gfx_aaf_header_t header = {};
            if (gfx_aaf_parse_header(frame_data, frame_size, &header) != GFX_AAF_FORMAT_SBMP) continue;
            // ESP_LOGD(TAG, "scaled: frame=%d src=%dx%d blocks=%d bh=%d bd=%d", fi, header.width, header.height, header.blocks, header.block_height, header.bit_depth);

            // 构建整帧 RGB565 缓冲
            int src_w = header.width;
            int src_h = header.height;
            std::unique_ptr<uint16_t[]> full(new uint16_t[src_w * src_h]);
            std::unique_ptr<uint32_t[]> offsets(new uint32_t[header.blocks]);
            gfx_aaf_calculate_offsets(&header, offsets.get());

            // 解码每个块到 full 缓冲
            std::unique_ptr<uint8_t[]> decode_buf;
            size_t decode_len = 0;
            if (header.bit_depth == 4) decode_len = src_w * (header.block_height + (header.block_height % 2)) / 2;
            else if (header.bit_depth == 8) decode_len = src_w * header.block_height;
            else if (header.bit_depth == 24) decode_len = src_w * header.block_height * 2;
            decode_buf.reset(new uint8_t[decode_len]);

            std::unique_ptr<uint32_t[]> palette;
            if (header.bit_depth != 24) {
                int colors = header.bit_depth == 4 ? 16 : 256;
                palette.reset(new uint32_t[colors]);
                for (int i = 0; i < colors; ++i) palette[i] = 0xFFFFFFFF;
            }

            for (int block = 0; block < header.blocks; ++block) {
                const uint8_t* block_data = frame_data + offsets[block];
                int block_len = header.block_len[block];
                uint8_t enc = block_data[0];
                if (enc == 0) { // RLE
                    size_t out_len = src_w * header.block_height;
                    gfx_aaf_rle_decode(block_data + 1, block_len - 1, decode_buf.get(), out_len);
                } else if (enc == 1 || enc == 3) { // Huffman / direct
                    size_t out_len = src_w * header.block_height;
                    if (enc == 1) {
                        std::unique_ptr<uint8_t[]> tmp(new uint8_t[out_len]);
                        size_t hlen = 0; gfx_aaf_huffman_decode(block_data, block_len, tmp.get(), &hlen);
                        gfx_aaf_rle_decode(tmp.get(), hlen, decode_buf.get(), out_len);
                    } else {
                        size_t hlen = 0; gfx_aaf_huffman_decode(block_data, block_len, decode_buf.get(), &hlen);
                    }
                } else if (enc == 2) {
                    uint32_t w, h; gfx_jpeg_decode(block_data + 1, block_len - 1, decode_buf.get(), src_w * header.block_height * 2, &w, &h, false);
                }

                int y0 = block * header.block_height;
                int y1 = (block == header.blocks - 1) ? src_h : (block + 1) * header.block_height;
                for (int y = y0; y < y1; ++y) {
                    int row = y - y0;
                    for (int x = 0; x < src_w; ++x) {
                        uint16_t rgb565 = 0;
                        if (header.bit_depth == 24) {
                            uint16_t* p = (uint16_t*)decode_buf.get();
                            rgb565 = p[row * src_w + x];
                        } else if (header.bit_depth == 8) {
                            uint8_t idx = decode_buf[row * src_w + x];
                            if (palette[idx] == 0xFFFFFFFF) { gfx_color_t c = gfx_aaf_parse_palette(&header, idx, true); palette[idx] = c.full; }
                            rgb565 = (uint16_t)palette[idx];
                        } else {
                            int bi = row * (src_w / 2) + (x / 2);
                            uint8_t packed = decode_buf[bi];
                            uint8_t idx = (x & 1) ? (packed & 0x0F) : ((packed & 0xF0) >> 4);
                            if (palette[idx] == 0xFFFFFFFF) { gfx_color_t c = gfx_aaf_parse_palette(&header, idx, true); palette[idx] = c.full; }
                            rgb565 = (uint16_t)palette[idx];
                        }
                        full[y * src_w + x] = rgb565;
                    }
                }
            }

            // 最近邻缩放到目标尺寸（使用 DMA 可用内存，确保 SPI 传输可靠），复用静态缓冲避免 DMA 未完成即释放
            static uint16_t* scaled = nullptr;
            static size_t scaled_cap = 0;
            size_t need_pixels = (size_t)target_eye_w * (size_t)target_eye_h;
            if (scaled_cap < need_pixels) {
                if (scaled) heap_caps_free(scaled);
                scaled = (uint16_t*)heap_caps_malloc(need_pixels * sizeof(uint16_t), MALLOC_CAP_DMA);
                scaled_cap = need_pixels;
            }
            if (!scaled) {
                // ESP_LOGE(TAG, "scaled: DMA malloc failed %u bytes", (unsigned)(target_eye_w * target_eye_h * sizeof(uint16_t)));
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
            for (int dy = 0; dy < target_eye_h; ++dy) {
                int sy = (int)((int64_t)dy * src_h / target_eye_h);
                for (int dx = 0; dx < target_eye_w; ++dx) {
                    int sx = (int)((int64_t)dx * src_w / target_eye_w);
                    scaled[dy * target_eye_w + dx] = full[sy * src_w + sx];
                }
            }

            // 改为在 OnFlush 做合成，避免与 GFX 刷新抢占导致闪烁
            g_eye_w = target_eye_w;
            g_eye_h = target_eye_h;
            g_eye_left_x = left_x;
            g_eye_left_y = center_y;
            g_eye_right_x = right_x;
            g_eye_right_y = center_y;
            // 复制一份缩放帧给全局缓冲（双眼用同一帧）
            if (!g_eye_frame || (size_t)(g_eye_w * g_eye_h) > scaled_cap) {
                if (g_eye_frame) heap_caps_free(g_eye_frame);
                g_eye_frame = (uint16_t*)heap_caps_malloc((size_t)g_eye_w * (size_t)g_eye_h * sizeof(uint16_t), MALLOC_CAP_DMA);
            }
            if (g_eye_frame) {
                memcpy(g_eye_frame, scaled, (size_t)g_eye_w * (size_t)g_eye_h * sizeof(uint16_t));
            }

            int fps = current_fps_;
            if (fps < 1) fps = 1;
            int delay_ms = 1000 / fps;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            if (!current_repeat_ && fi == total_frames - 1) break;
        }
    }
}

} // namespace anim
