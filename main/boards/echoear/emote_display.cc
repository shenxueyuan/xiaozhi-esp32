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
// 标记时间/文案可见性（用于仅镜像这两者）
static bool g_label_time_visible = false;
static bool g_label_tips_visible = false;

// 方案1全局缩放帧（供 OnFlush 合成，避免与 GFX 刷新打架导致闪烁）
static uint16_t* g_eye_frame = nullptr; // 持久 DMA 缓冲，存放单眼缩放后的 RGB565
static int g_eye_w = 0;
static int g_eye_h = 0;
static int g_eye_left_x = 0;
static int g_eye_left_y = 0;
static int g_eye_right_x = 0;
static int g_eye_right_y = 0;
// 叠加绘制所需的裁剪临时缓冲，保证子矩形数据连续
static uint16_t* g_eye_subbuf = nullptr;
static size_t g_eye_subcap = 0;
// 全屏离屏合成缓冲（PSRAM），以及行DMA缓冲（内部内存）
static uint16_t* g_full_frame = nullptr;
static size_t g_full_frame_cap = 0;
static uint16_t* g_line_buf = nullptr;
static size_t g_line_buf_cap = 0;

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
    g_label_time_visible = false;
    g_label_tips_visible = false;

    // Show the selected control
    switch (mode) {
    case UIDisplayMode::SHOW_ANIM_TOP:
        gfx_obj_set_visible(obj_anim_mic, true);
        break;
    case UIDisplayMode::SHOW_TIME:
        gfx_obj_set_visible(obj_label_time, true);
        g_label_time_visible = true;
        break;
    case UIDisplayMode::SHOW_TIPS:
        gfx_obj_set_visible(obj_label_tips, true);
        g_label_tips_visible = true;
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
            .buf_pixels = DISPLAY_WIDTH * 32,
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
    const int target_eye_w = 115;
    const int target_eye_h = 115;
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
        int fw = x_end - x_start;
        int fh = y_end - y_start;
        // 1) 将当前 tile 拷入全屏离屏缓冲
        size_t full_need = (size_t)DISPLAY_WIDTH * DISPLAY_HEIGHT;
        if (g_full_frame_cap < full_need) {
            if (g_full_frame) heap_caps_free(g_full_frame);
            g_full_frame = (uint16_t*)heap_caps_malloc(full_need * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
            g_full_frame_cap = full_need;
        }
        if (g_full_frame) {
            for (int row = 0; row < fh; ++row) {
                memcpy(g_full_frame + (y_start + row) * DISPLAY_WIDTH + x_start,
                       (const uint16_t*)color_data + row * fw,
                       (size_t)fw * sizeof(uint16_t));
            }

            // 2) 如果这是最后一个 tile（覆盖到右下角），则叠加左右眼，并在需要时对时间/文案做镜像，然后整屏一次刷新
            if (x_end >= (int)DISPLAY_WIDTH && y_end >= (int)DISPLAY_HEIGHT) {
                if (g_eye_frame && g_eye_w > 0 && g_eye_h > 0) {
                    // 定义时间/文案矩形用于遮罩（防止眼睛覆盖其像素，保证文字在最上层）
                    auto in_label_region = [&](int px, int py)->bool {
                        bool in_time = false, in_tips = false;
                        if (g_label_time_visible) {
                            int x = (int)(DISPLAY_WIDTH/2 - 70), y = 32, w = 140, h = 46;
                            in_time = (px >= x && px < x + w && py >= y && py < y + h);
                        }
                        if (g_label_tips_visible) {
                            int x = (int)(DISPLAY_WIDTH/2 - 70), y = 50, w = 140, h = 36;
                            in_tips = (px >= x && px < x + w && py >= y && py < y + h);
                        }
                        return in_time || in_tips;
                    };
                    // 左眼覆盖（不镜像）
                    int lx1 = g_eye_left_x, ly1 = g_eye_left_y;
                    int lx2 = lx1 + g_eye_w, ly2 = ly1 + g_eye_h;
                    int ix1 = std::max(lx1, 0);
                    int iy1 = std::max(ly1, 0);
                    int ix2 = std::min(lx2, (int)DISPLAY_WIDTH);
                    int iy2 = std::min(ly2, (int)DISPLAY_HEIGHT);
                    if (ix1 < ix2 && iy1 < iy2) {
                        int sub_w = ix2 - ix1;
                        int sub_h = iy2 - iy1;
                        int sx0 = ix1 - lx1;
                        int sy0 = iy1 - ly1;
                        for (int row = 0; row < sub_h; ++row) {
                            uint16_t* dst_row = g_full_frame + ((iy1 + row) * DISPLAY_WIDTH + ix1);
                            const uint16_t* src_row = g_eye_frame + ((sy0 + row) * g_eye_w + sx0);
                            for (int col = 0; col < sub_w; ++col) {
                                int px = ix1 + col; int py = iy1 + row;
                                if (in_label_region(px, py)) { continue; }
                                uint16_t fg = src_row[col];
                                uint16_t bg = dst_row[col];
                                // 边缘检测：与右/下像素差异较大则做50%混合
                                uint16_t fg_next_x = src_row[std::min(col + 1, sub_w - 1)];
                                uint16_t fg_next_y = src_row[col];
                                if (row + 1 < sub_h) fg_next_y = *(src_row + g_eye_w + col);
                                int drx = (int)(fg >> 11) - (int)(fg_next_x >> 11);
                                int dgx = (int)((fg >> 5) & 0x3F) - (int)((fg_next_x >> 5) & 0x3F);
                                int dbx = (int)(fg & 0x1F) - (int)(fg_next_x & 0x1F);
                                int dry = (int)(fg >> 11) - (int)(fg_next_y >> 11);
                                int dgy = (int)((fg >> 5) & 0x3F) - (int)((fg_next_y >> 5) & 0x3F);
                                int dby = (int)(fg & 0x1F) - (int)(fg_next_y & 0x1F);
                                int edge = (abs(drx) + abs(dgx) + abs(dbx) + abs(dry) + abs(dgy) + abs(dby));
                                if (edge > 12) { // 阈值：经验值，避免整体模糊
                                    int r = ((fg >> 11) & 0x1F) + ((bg >> 11) & 0x1F);
                                    int g = ((fg >> 5) & 0x3F) + ((bg >> 5) & 0x3F);
                                    int b = (fg & 0x1F) + (bg & 0x1F);
                                    dst_row[col] = (uint16_t)(((r >> 1) << 11) | ((g >> 1) << 5) | (b >> 1));
                                } else {
                                    dst_row[col] = fg;
                                }
                            }
                        }
                    }
                    // 右眼覆盖（水平镜像）
                    int rx1 = g_eye_right_x, ry1 = g_eye_right_y;
                    int rx2 = rx1 + g_eye_w, ry2 = ry1 + g_eye_h;
                    int jx1 = std::max(rx1, 0);
                    int jy1 = std::max(ry1, 0);
                    int jx2 = std::min(rx2, (int)DISPLAY_WIDTH);
                    int jy2 = std::min(ry2, (int)DISPLAY_HEIGHT);
                    if (jx1 < jx2 && jy1 < jy2) {
                        int sub_w = jx2 - jx1;
                        int sub_h = jy2 - jy1;
                        int sx0 = jx1 - rx1;
                        int sy0 = jy1 - ry1;
                        for (int row = 0; row < sub_h; ++row) {
                            uint16_t* dst_row = g_full_frame + ((jy1 + row) * DISPLAY_WIDTH + jx1);
                            const uint16_t* src_row_base = g_eye_frame + ((sy0 + row) * g_eye_w);
                            for (int col = 0; col < sub_w; ++col) {
                                int px = jx1 + col; int py = jy1 + row;
                                if (in_label_region(px, py)) { continue; }
                                int sx = sx0 + col;
                                int sx_m = g_eye_w - 1 - sx; // 水平镜像采样位置
                                uint16_t fg = src_row_base[sx_m];
                                uint16_t bg = dst_row[col];
                                int sx_m_next = (sx_m > 0) ? (sx_m - 1) : sx_m;
                                uint16_t fg_next_x = src_row_base[sx_m_next];
                                const uint16_t* src_row_base_next = (row + 1 < sub_h) ? (src_row_base + g_eye_w) : src_row_base;
                                uint16_t fg_next_y = src_row_base_next[sx_m];
                                int drx = (int)(fg >> 11) - (int)(fg_next_x >> 11);
                                int dgx = (int)((fg >> 5) & 0x3F) - (int)((fg_next_x >> 5) & 0x3F);
                                int dbx = (int)(fg & 0x1F) - (int)(fg_next_x & 0x1F);
                                int dry = (int)(fg >> 11) - (int)(fg_next_y >> 11);
                                int dgy = (int)((fg >> 5) & 0x3F) - (int)((fg_next_y >> 5) & 0x3F);
                                int dby = (int)(fg & 0x1F) - (int)(fg_next_y & 0x1F);
                                int edge = (abs(drx) + abs(dgx) + abs(dbx) + abs(dry) + abs(dgy) + abs(dby));
                                if (edge > 12) {
                                    int r = ((fg >> 11) & 0x1F) + ((bg >> 11) & 0x1F);
                                    int g = ((fg >> 5) & 0x3F) + ((bg >> 5) & 0x3F);
                                    int b = (fg & 0x1F) + (bg & 0x1F);
                                    dst_row[col] = (uint16_t)(((r >> 1) << 11) | ((g >> 1) << 5) | (b >> 1));
                                } else {
                                    dst_row[col] = fg;
                                }
                            }
                        }
                    }
                }
                // 仅当时间/文案可见时，对其区域做水平镜像
                if (g_label_time_visible || g_label_tips_visible) {
                    // 获取两者的外接矩形（简单起见固定区域：时间在 y≈32、高≈46；文案在 y≈50、高≈36；与 InitializeLabels 对齐）
                    auto mirror_region = [&](int x, int y, int w, int h) {
                        int mx1 = std::max(0, x);
                        int my1 = std::max(0, y);
                        int mx2 = std::min((int)DISPLAY_WIDTH, x + w);
                        int my2 = std::min((int)DISPLAY_HEIGHT, y + h);
                        for (int row = my1; row < my2; ++row) {
                            uint16_t* line = g_full_frame + row * DISPLAY_WIDTH;
                            int l = mx1, r = mx2 - 1;
                            while (l < r) {
                                uint16_t tmp = line[l];
                                line[l] = line[r];
                                line[r] = tmp;
                                l++; r--;
                            }
                        }
                    };
                    // 为避免底部裁切，向下各扩展 4px（不越界）
                    auto clamp_h = [&](int y, int h){ return std::min(h + 4, (int)DISPLAY_HEIGHT - y); };
                    if (g_label_time_visible) { int y=32; mirror_region( (int)(DISPLAY_WIDTH/2 - 70), y, 140, clamp_h(y,46) ); }
                    if (g_label_tips_visible) { int y=50; mirror_region( (int)(DISPLAY_WIDTH/2 - 70), y, 140, clamp_h(y,36) ); }
                }

                // 3) 行缓冲推送整屏（避免 tile 边界缝隙）
                size_t line_need = (size_t)DISPLAY_WIDTH;
                if (g_line_buf_cap < line_need) {
                    if (g_line_buf) heap_caps_free(g_line_buf);
                    g_line_buf = (uint16_t*)heap_caps_malloc(line_need * sizeof(uint16_t), MALLOC_CAP_DMA);
                    g_line_buf_cap = line_need;
                }
                if (g_line_buf) {
                    for (int y = 0; y < (int)DISPLAY_HEIGHT; ++y) {
                        memcpy(g_line_buf, g_full_frame + y * DISPLAY_WIDTH, DISPLAY_WIDTH * sizeof(uint16_t));
                        esp_lcd_panel_draw_bitmap(panel, 0, y, DISPLAY_WIDTH, y + 1, g_line_buf);
                    }
                } else {
                    // 退化：直接整屏一次刷
                    esp_lcd_panel_draw_bitmap(panel, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, g_full_frame);
                }
            }
        } else {
            // 无 full frame 时退回原逻辑
            esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, color_data);
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
    const int target_eye_w = 115;
    const int target_eye_h = 115;
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
            // 使用双线性缩放，缓解像素格栅感
            for (int dy = 0; dy < target_eye_h; ++dy) {
                int sy_fp = (int)((int64_t)dy * (src_h - 1) * 256 / (target_eye_h - 1));
                int sy = sy_fp >> 8; int fy = sy_fp & 0xFF;
                int sy1 = (sy + 1 < src_h) ? sy + 1 : sy;
                for (int dx = 0; dx < target_eye_w; ++dx) {
                    int sx_fp = (int)((int64_t)dx * (src_w - 1) * 256 / (target_eye_w - 1));
                    int sx = sx_fp >> 8; int fx = sx_fp & 0xFF;
                    int sx1 = (sx + 1 < src_w) ? sx + 1 : sx;

                    uint16_t c00 = full[sy * src_w + sx];
                    uint16_t c10 = full[sy * src_w + sx1];
                    uint16_t c01 = full[sy1 * src_w + sx];
                    uint16_t c11 = full[sy1 * src_w + sx1];

                    int r00 = (c00 >> 11) & 0x1F, g00 = (c00 >> 5) & 0x3F, b00 = c00 & 0x1F;
                    int r10 = (c10 >> 11) & 0x1F, g10 = (c10 >> 5) & 0x3F, b10 = c10 & 0x1F;
                    int r01 = (c01 >> 11) & 0x1F, g01 = (c01 >> 5) & 0x3F, b01 = c01 & 0x1F;
                    int r11 = (c11 >> 11) & 0x1F, g11 = (c11 >> 5) & 0x3F, b11 = c11 & 0x1F;

                    int w00 = (256 - fx) * (256 - fy);
                    int w10 = fx * (256 - fy);
                    int w01 = (256 - fx) * fy;
                    int w11 = fx * fy;
                    int denom = 256 * 256;

                    int r = (r00 * w00 + r10 * w10 + r01 * w01 + r11 * w11 + (denom >> 1)) / denom;
                    int g = (g00 * w00 + g10 * w10 + g01 * w01 + g11 * w11 + (denom >> 1)) / denom;
                    int b = (b00 * w00 + b10 * w10 + b01 * w01 + b11 * w11 + (denom >> 1)) / denom;

                    if (r > 31) r = 31;
                    if (g > 63) g = 63;
                    if (b > 31) b = 31;
                    scaled[dy * target_eye_w + dx] = (uint16_t)((r << 11) | (g << 5) | b);
                }
            }

            // 不做全局模糊，改为在合成阶段进行边缘感知混合以保留细节

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
                // 优先内部内存，减少 SPI DMA 分块导致的行间缝隙；失败再回退 DMA
                g_eye_frame = (uint16_t*)heap_caps_malloc((size_t)g_eye_w * (size_t)g_eye_h * sizeof(uint16_t), MALLOC_CAP_INTERNAL);
                if (!g_eye_frame) {
                    g_eye_frame = (uint16_t*)heap_caps_malloc((size_t)g_eye_w * (size_t)g_eye_h * sizeof(uint16_t), MALLOC_CAP_DMA);
                }
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
