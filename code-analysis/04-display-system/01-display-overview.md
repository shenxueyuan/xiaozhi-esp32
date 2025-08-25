# 显示系统 (Display) 架构详解

## 📁 模块概览
- **文件数量**: 8个文件 (4个.h + 4个.cc)
- **功能**: 图形界面显示，基于LVGL图形库
- **设计模式**: 策略模式 + 锁机制 + 主题系统
- **支持类型**: LCD、OLED、ESP日志显示

## 🏗️ 架构设计

### 继承层次结构
```
Display (抽象基类)
├── LcdDisplay              # LCD TFT显示器
├── OledDisplay             # OLED显示器
├── EsplogDisplay           # ESP日志显示
└── NoDisplay               # 无显示实现
```

## 🎯 Display基类分析

### 核心接口定义
```cpp
class Display {
public:
    Display();
    virtual ~Display();

    // 内容显示接口
    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void SetEmotion(const char* emotion);
    virtual void SetChatMessage(const char* role, const char* content);
    virtual void SetIcon(const char* icon);
    virtual void SetPreviewImage(const lv_img_dsc_t* image);

    // 主题和状态管理
    virtual void SetTheme(const std::string& theme_name);
    virtual std::string GetTheme() { return current_theme_name_; }
    virtual void UpdateStatusBar(bool update_all = false);
    virtual void SetPowerSaveMode(bool on);

    // 尺寸信息
    inline int width() const { return width_; }
    inline int height() const { return height_; }

protected:
    // 显示参数
    int width_ = 0;
    int height_ = 0;

    // LVGL和电源管理
    esp_pm_lock_handle_t pm_lock_ = nullptr;
    lv_display_t *display_ = nullptr;

    // UI组件
    lv_obj_t *emotion_label_ = nullptr;         // 表情显示
    lv_obj_t *network_label_ = nullptr;         // 网络状态
    lv_obj_t *status_label_ = nullptr;          // 状态文本
    lv_obj_t *notification_label_ = nullptr;    // 通知消息
    lv_obj_t *mute_label_ = nullptr;            // 静音指示
    lv_obj_t *battery_label_ = nullptr;         // 电池电量
    lv_obj_t* chat_message_label_ = nullptr;    // 聊天消息
    lv_obj_t* low_battery_popup_ = nullptr;     // 低电量弹窗

    // 状态变量
    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;
    bool muted_ = false;
    std::string current_theme_name_;

    // 定时器
    std::chrono::system_clock::time_point last_status_update_time_;
    esp_timer_handle_t notification_timer_ = nullptr;

    // 纯虚函数，由子类实现
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;
};
```

### 字体系统设计
```cpp
struct DisplayFonts {
    const lv_font_t* text_font = nullptr;      // 文本字体
    const lv_font_t* icon_font = nullptr;      // 图标字体
    const lv_font_t* emoji_font = nullptr;     // 表情字体
};
```

### 线程安全锁机制
```cpp
class DisplayLockGuard {
public:
    DisplayLockGuard(Display *display) : display_(display) {
        if (!display_->Lock(30000)) {           // 30秒超时
            ESP_LOGE("Display", "Failed to lock display");
        }
    }

    ~DisplayLockGuard() {
        display_->Unlock();
    }

private:
    Display *display_;
};

// 使用方式
void SomeFunction() {
    DisplayLockGuard lock(display);  // 自动加锁
    // 进行显示操作
    display->SetStatus("Processing...");
}  // 自动解锁
```

## 📺 LcdDisplay 详细分析

### LCD显示器特点
- **支持类型**: TFT LCD (ILI9341, ST7789等)
- **接口**: SPI接口
- **分辨率**: 240x240, 320x240, 480x320等
- **颜色**: 16位RGB565格式

### 核心实现
```cpp
class LcdDisplay : public Display {
private:
    // SPI和LCD控制
    spi_device_handle_t spi_device_ = nullptr;
    gpio_num_t dc_pin_;                    // 数据/命令控制引脚
    gpio_num_t reset_pin_;                 // 复位引脚
    gpio_num_t cs_pin_;                    // 片选引脚

    // LCD驱动参数
    lcd_panel_handle_t lcd_panel_ = nullptr;
    bool mirror_x_ = false;
    bool mirror_y_ = false;
    bool swap_xy_ = false;
    bool invert_color_ = false;

    // LVGL缓冲区
    lv_color_t* lvgl_buffer1_ = nullptr;
    lv_color_t* lvgl_buffer2_ = nullptr;
    size_t buffer_size_;

    // 同步机制
    SemaphoreHandle_t lvgl_mux_ = nullptr;
    bool display_ready_ = false;

public:
    LcdDisplay(int width, int height,
               gpio_num_t mosi_pin, gpio_num_t sclk_pin,
               gpio_num_t dc_pin, gpio_num_t reset_pin = GPIO_NUM_NC,
               gpio_num_t cs_pin = GPIO_NUM_NC,
               bool mirror_x = false, bool mirror_y = false,
               bool swap_xy = false, bool invert_color = false);

    virtual ~LcdDisplay();

protected:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

private:
    void InitializeSpi();
    void InitializeLcdPanel();
    void InitializeLvgl();
    static void LvglFlushCallback(lv_display_t* display,
                                 const lv_area_t* area,
                                 uint8_t* color_map);
};
```

### LCD初始化流程
```cpp
void LcdDisplay::InitializeLcdPanel() {
    // 1. 创建SPI总线配置
    spi_bus_config_t bus_config = {
        .mosi_io_num = mosi_pin_,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = sclk_pin_,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = width_ * height_ * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_config, SPI_DMA_CH_AUTO));

    // 2. 创建LCD面板配置
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = dc_pin_,
        .cs_gpio_num = cs_pin_,
        .pclk_hz = 80 * 1000 * 1000,        // 80MHz SPI时钟
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,                // RGB565
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    esp_lcd_panel_io_handle_t io_handle;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(&io_config, &io_handle));

    // 3. 创建LCD面板
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = reset_pin_,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(&panel_config, io_handle, &lcd_panel_));

    // 4. 初始化面板
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_panel_, invert_color_));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_panel_, mirror_x_, mirror_y_));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(lcd_panel_, swap_xy_));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel_, true));
}
```

### LVGL集成
```cpp
void LcdDisplay::InitializeLvgl() {
    // 1. 分配显示缓冲区
    buffer_size_ = width_ * height_ / 10;  // 缓冲区大小为屏幕的1/10
    lvgl_buffer1_ = (lv_color_t*)heap_caps_malloc(
        buffer_size_ * sizeof(lv_color_t),
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );
    lvgl_buffer2_ = (lv_color_t*)heap_caps_malloc(
        buffer_size_ * sizeof(lv_color_t),
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );

    // 2. 初始化LVGL显示驱动
    lv_init();
    display_ = lv_display_create(width_, height_);
    lv_display_set_flush_cb(display_, LvglFlushCallback);
    lv_display_set_buffers(display_, lvgl_buffer1_, lvgl_buffer2_,
                          buffer_size_, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(display_, this);

    // 3. 创建互斥锁
    lvgl_mux_ = xSemaphoreCreateMutex();

    // 4. 启动LVGL定时器任务
    const esp_timer_create_args_t lvgl_timer_args = {
        .callback = &LvglTimerCallback,
        .arg = this,
        .name = "lvgl_timer"
    };
    esp_timer_handle_t lvgl_timer;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_timer_args, &lvgl_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_timer, 5000)); // 5ms周期
}
```

### 显示刷新回调
```cpp
static void LcdDisplay::LvglFlushCallback(lv_display_t* display,
                                         const lv_area_t* area,
                                         uint8_t* color_map) {
    LcdDisplay* lcd_display = (LcdDisplay*)lv_display_get_user_data(display);

    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2;
    int y2 = area->y2;

    // 将LVGL的颜色数据发送到LCD
    esp_lcd_panel_draw_bitmap(lcd_display->lcd_panel_,
                             x1, y1, x2 + 1, y2 + 1,
                             color_map);

    // 通知LVGL刷新完成
    lv_display_flush_ready(display);
}
```

## 🖥️ OledDisplay 分析

### OLED显示器特点
```cpp
class OledDisplay : public Display {
private:
    // I2C接口
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t i2c_device_;
    uint8_t i2c_address_;

    // OLED参数
    int oled_width_;
    int oled_height_;
    bool external_vcc_;

    // 显示缓冲区
    uint8_t* oled_buffer_;
    size_t buffer_size_;

    // 同步机制
    SemaphoreHandle_t oled_mux_ = nullptr;

public:
    OledDisplay(int width, int height,
                i2c_master_bus_handle_t i2c_bus,
                uint8_t i2c_address = 0x3C,
                bool external_vcc = false);

private:
    void InitializeOled();
    void SendCommand(uint8_t command);
    void SendData(uint8_t* data, size_t length);
    void UpdateDisplay();
    static void LvglFlushCallback(lv_display_t* display,
                                 const lv_area_t* area,
                                 uint8_t* color_map);
};
```

### OLED初始化序列
```cpp
void OledDisplay::InitializeOled() {
    // SSD1306 OLED初始化序列
    SendCommand(0xAE);  // Display OFF
    SendCommand(0x20);  // Set Memory Addressing Mode
    SendCommand(0x00);  // Horizontal Addressing Mode
    SendCommand(0xB0);  // Set Page Start Address
    SendCommand(0xC8);  // Set COM Output Scan Direction
    SendCommand(0x00);  // Set Low Column Address
    SendCommand(0x10);  // Set High Column Address
    SendCommand(0x40);  // Set Start Line Address
    SendCommand(0x81);  // Set Contrast Control
    SendCommand(0xFF);  // Maximum Contrast
    SendCommand(0xA1);  // Set Segment Re-map
    SendCommand(0xA6);  // Set Normal Display
    SendCommand(0xA8);  // Set Multiplex Ratio
    SendCommand(oled_height_ - 1);
    SendCommand(0xA4);  // Output follows RAM content
    SendCommand(0xD3);  // Set Display Offset
    SendCommand(0x00);  // No offset
    SendCommand(0xD5);  // Set Display Clock Divide Ratio
    SendCommand(0xF0);  // Suggested ratio
    SendCommand(0xD9);  // Set Pre-charge Period
    SendCommand(external_vcc_ ? 0x22 : 0xF1);
    SendCommand(0xDA);  // Set COM Pins Hardware Configuration
    SendCommand((oled_height_ == 32) ? 0x02 : 0x12);
    SendCommand(0xDB);  // Set VCOMH Deselect Level
    SendCommand(0x40);
    SendCommand(0x8D);  // Charge Pump Setting
    SendCommand(external_vcc_ ? 0x10 : 0x14);
    SendCommand(0xAF);  // Display ON
}
```

## 📝 EsplogDisplay 分析

### 日志显示实现
```cpp
class EsplogDisplay : public Display {
private:
    bool log_enabled_ = true;
    std::vector<std::string> log_buffer_;
    size_t max_log_lines_ = 100;
    SemaphoreHandle_t log_mux_ = nullptr;

public:
    EsplogDisplay();

    // 重写显示方法输出到ESP日志
    virtual void SetStatus(const char* status) override {
        ESP_LOGI("Display", "Status: %s", status);
        AddToLogBuffer(std::string("Status: ") + status);
    }

    virtual void ShowNotification(const char* notification, int duration_ms) override {
        ESP_LOGI("Display", "Notification: %s", notification);
        AddToLogBuffer(std::string("Notification: ") + notification);
    }

    virtual void SetEmotion(const char* emotion) override {
        ESP_LOGI("Display", "Emotion: %s", emotion);
        AddToLogBuffer(std::string("Emotion: ") + emotion);
    }

private:
    void AddToLogBuffer(const std::string& message);
    void PrintLogBuffer();
};
```

## 🎨 主题系统

### 主题配置结构
```cpp
struct DisplayTheme {
    // 颜色配置
    lv_color_t background_color;
    lv_color_t text_color;
    lv_color_t accent_color;
    lv_color_t status_bar_color;

    // 字体配置
    const lv_font_t* title_font;
    const lv_font_t* text_font;
    const lv_font_t* small_font;

    // 布局配置
    int status_bar_height;
    int margin_size;
    int border_radius;

    // 动画配置
    int animation_duration;
    lv_anim_path_t animation_path;
};

// 预定义主题
static const DisplayTheme kLightTheme = {
    .background_color = lv_color_white(),
    .text_color = lv_color_black(),
    .accent_color = lv_color_make(0x21, 0x96, 0xF3),
    // ... 其他配置
};

static const DisplayTheme kDarkTheme = {
    .background_color = lv_color_black(),
    .text_color = lv_color_white(),
    .accent_color = lv_color_make(0xBB, 0x86, 0xFC),
    // ... 其他配置
};
```

### 主题切换实现
```cpp
void Display::SetTheme(const std::string& theme_name) {
    DisplayLockGuard lock(this);

    const DisplayTheme* theme = nullptr;

    if (theme_name == "light") {
        theme = &kLightTheme;
    } else if (theme_name == "dark") {
        theme = &kDarkTheme;
    } else {
        ESP_LOGW(TAG, "Unknown theme: %s", theme_name.c_str());
        return;
    }

    current_theme_name_ = theme_name;

    // 应用主题到所有UI组件
    ApplyThemeToObject(lv_screen_active(), theme);

    // 保存主题设置
    Settings settings("display", true);
    settings.SetString("theme", theme_name);

    ESP_LOGI(TAG, "Theme changed to: %s", theme_name.c_str());
}

void Display::ApplyThemeToObject(lv_obj_t* obj, const DisplayTheme* theme) {
    // 设置背景颜色
    lv_obj_set_style_bg_color(obj, theme->background_color, 0);

    // 设置文本颜色
    lv_obj_set_style_text_color(obj, theme->text_color, 0);

    // 设置字体
    lv_obj_set_style_text_font(obj, theme->text_font, 0);

    // 递归应用到子对象
    for (int i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t* child = lv_obj_get_child(obj, i);
        ApplyThemeToObject(child, theme);
    }
}
```

## ⚡ 性能优化

### 内存管理优化
```cpp
// DMA内存分配
lvgl_buffer1_ = (lv_color_t*)heap_caps_malloc(
    buffer_size_ * sizeof(lv_color_t),
    MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL  // DMA兼容内存
);

// 双缓冲减少撕裂
lv_display_set_buffers(display_, lvgl_buffer1_, lvgl_buffer2_,
                       buffer_size_, LV_DISPLAY_RENDER_MODE_PARTIAL);
```

### 渲染优化
```cpp
// 部分刷新模式
LV_DISPLAY_RENDER_MODE_PARTIAL  // 只刷新变化区域

// 异步刷新
static void LvglTimerCallback(void* arg) {
    LcdDisplay* display = (LcdDisplay*)arg;
    if (display->Lock(0)) {  // 非阻塞锁
        lv_timer_handler();  // 处理LVGL定时器
        display->Unlock();
    }
}
```

### 功耗优化
```cpp
void Display::SetPowerSaveMode(bool on) {
    if (on) {
        // 降低显示亮度
        SetBacklightBrightness(20);

        // 降低刷新率
        esp_timer_stop(lvgl_timer_);
        esp_timer_start_periodic(lvgl_timer_, 16000); // 60Hz -> 60Hz

        // 获取电源管理锁
        if (pm_lock_ == nullptr) {
            esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "display", &pm_lock_);
        }
    } else {
        // 恢复正常模式
        SetBacklightBrightness(80);
        esp_timer_stop(lvgl_timer_);
        esp_timer_start_periodic(lvgl_timer_, 5000);  // 200Hz

        // 释放电源管理锁
        if (pm_lock_) {
            esp_pm_lock_release(pm_lock_);
        }
    }
}
```

## 🔗 与其他模块集成

### Application集成
```cpp
void Application::UpdateDisplay() {
    auto* display = Board::GetInstance().GetDisplay();

    // 更新设备状态
    const char* status = GetDeviceStateString(device_state_);
    display->SetStatus(status);

    // 更新网络状态
    display->UpdateStatusBar();

    // 显示聊天消息
    if (!current_chat_message_.empty()) {
        display->SetChatMessage("user", current_chat_message_.c_str());
    }
}
```

### 多语言支持
```cpp
void Display::SetLocalizedText(const char* key) {
    std::string localized = Lang::GetString(key);
    SetStatus(localized.c_str());
}

// 使用示例
display->SetLocalizedText("status.connecting");  // 显示本地化的"连接中"
```

---

**相关文档**:
- [Board硬件抽象层](../05-board-abstraction/01-board-base.md)
- [LVGL配置和字体](./02-lvgl-integration.md)
- [UI主题和样式](./03-ui-themes.md)
