# EchoEar 喵伴 硬件功能操作手册

## 产品概述

EchoEar 喵伴是一款智能 AI 开发套件，搭载 ESP32-S3-WROOM-1 模组，配备1.85寸QSPI圆形触摸屏，双麦阵列，支持离线语音唤醒与声源定位算法。该设备专为智能助手应用场景设计，提供丰富的人机交互体验。

### 硬件规格
- **主控芯片**: ESP32-S3-WROOM-1
- **显示屏**: 1.85寸 QSPI 圆形触摸屏 (360×360像素)
- **音频系统**: 双麦阵列 + 功放
- **存储**: 16MB Flash
- **连接**: WiFi + 可选4G模块
- **电源**: 锂电池供电

## 硬件接口配置

### 1. 音频系统配置

#### I2S音频接口
- **MCLK**: GPIO42 (主时钟)
- **BCLK**: GPIO40 (位时钟)
- **WS**: GPIO39 (字选择)
- **DOUT**: GPIO41 (音频输出)
- **DIN**:
  - PCB V1.0: GPIO15
  - PCB V1.2: GPIO3

**对应代码** (`config.h`):
```c
#define AUDIO_I2S_GPIO_MCLK     GPIO_NUM_42
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_39
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_40
#define AUDIO_I2S_GPIO_DIN_1    GPIO_NUM_15
#define AUDIO_I2S_GPIO_DIN_2    GPIO_NUM_3
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_41
```

#### 音频编解码器
- **I2C接口**:
  - SDA: GPIO2
  - SCL: GPIO1
- **功放控制**:
  - PCB V1.0: GPIO4
  - PCB V1.2: GPIO15
- **编解码器地址**:
  - ES8311 (播放): 默认地址
  - ES7210 (录音): 默认地址

**对应代码** (`config.h`):
```c
#define AUDIO_CODEC_PA_PIN_1    GPIO_NUM_4
#define AUDIO_CODEC_PA_PIN_2     GPIO_NUM_15
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_2
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_1
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR
```

#### 音频参数
- **采样率**: 24kHz (输入/输出)
- **参考电压**: 启用

**对应代码** (`config.h`):
```c
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_INPUT_REFERENCE    true
```

#### 音频编解码器初始化代码
**对应代码** (`EchoEar.cc`):
```cpp
virtual AudioCodec* GetAudioCodec() override
{
    static BoxAudioCodec audio_codec(
        i2c_bus_,
        AUDIO_INPUT_SAMPLE_RATE,
        AUDIO_OUTPUT_SAMPLE_RATE,
        AUDIO_I2S_GPIO_MCLK,
        AUDIO_I2S_GPIO_BCLK,
        AUDIO_I2S_GPIO_WS,
        AUDIO_I2S_GPIO_DOUT,
        AUDIO_I2S_GPIO_DIN,
        AUDIO_CODEC_PA_PIN,
        AUDIO_CODEC_ES8311_ADDR,
        AUDIO_CODEC_ES7210_ADDR,
        AUDIO_INPUT_REFERENCE);
    return &audio_codec;
}
```

### 2. 显示系统配置

#### QSPI LCD接口
- **时钟**: GPIO18
- **片选**: GPIO14
- **数据线**:
  - DATA0: GPIO46
  - DATA1: GPIO13
  - DATA2: GPIO11
  - DATA3: GPIO12
- **复位**:
  - PCB V1.0: GPIO3
  - PCB V1.2: GPIO47
- **背光**: GPIO44

**对应代码** (`config.h`):
```c
#define QSPI_LCD_HOST           SPI2_HOST
#define QSPI_PIN_NUM_LCD_PCLK   GPIO_NUM_18
#define QSPI_PIN_NUM_LCD_CS     GPIO_NUM_14
#define QSPI_PIN_NUM_LCD_DATA0  GPIO_NUM_46
#define QSPI_PIN_NUM_LCD_DATA1  GPIO_NUM_13
#define QSPI_PIN_NUM_LCD_DATA2  GPIO_NUM_11
#define QSPI_PIN_NUM_LCD_DATA3  GPIO_NUM_12
#define QSPI_PIN_NUM_LCD_RST_1  GPIO_NUM_3
#define QSPI_PIN_NUM_LCD_RST_2  GPIO_NUM_47
#define QSPI_PIN_NUM_LCD_BL     GPIO_NUM_44
```

#### 显示参数
- **分辨率**: 360×360像素
- **颜色深度**: 16位
- **镜像**: 无镜像
- **旋转**: 无旋转

**对应代码** (`config.h`):
```c
#define DISPLAY_WIDTH       360
#define DISPLAY_HEIGHT      360
#define DISPLAY_MIRROR_X    false
#define DISPLAY_MIRROR_Y    false
#define DISPLAY_SWAP_XY     false
#define QSPI_LCD_H_RES           (360)
#define QSPI_LCD_V_RES           (360)
#define QSPI_LCD_BIT_PER_PIXEL   (16)
```

#### 显示系统初始化代码
**对应代码** (`EchoEar.cc`):
```cpp
void Initializest77916Display(uint8_t pcb_verison)
{
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;

    const esp_lcd_panel_io_spi_config_t io_config = ST77916_PANEL_IO_QSPI_CONFIG(QSPI_PIN_NUM_LCD_CS, NULL, NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)QSPI_LCD_HOST, &io_config, &panel_io));

    st77916_vendor_config_t vendor_config = {
        .init_cmds = vendor_specific_init_yysj,
        .init_cmds_size = sizeof(vendor_specific_init_yysj) / sizeof(st77916_lcd_init_cmd_t),
        .flags = {
            .use_qspi_interface = 1,
        },
    };

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = QSPI_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = QSPI_LCD_BIT_PER_PIXEL,
        .flags = {
            .reset_active_high = pcb_verison,
        },
        .vendor_config = &vendor_config,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(panel_io, &panel_config, &panel));
    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_disp_on_off(panel, true);
    esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

#if USE_LVGL_DEFAULT
    display_ = new SpiLcdDisplay(panel_io, panel,
        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
#else
    display_ = new anim::EmoteDisplay(panel, panel_io);
#endif
    backlight_ = new PwmBacklight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
    backlight_->RestoreBrightness();
}
```

### 3. 触摸系统配置

#### CST816S触摸控制器
- **I2C地址**: 0x15
- **中断引脚**: GPIO10
- **复位引脚**: 未使用

**对应代码** (`config.h`):
```c
#define TP_PIN_NUM_INT   (GPIO_NUM_10)
#define TP_PIN_NUM_RST   (GPIO_NUM_NC)
```

#### 触摸功能
- 支持单点触摸
- 触摸事件类型：
  - 按下 (TOUCH_PRESS)
  - 释放 (TOUCH_RELEASE)
  - 长按 (TOUCH_HOLD)

**对应代码** (`EchoEar.cc`):
```cpp
class Cst816s : public I2cDevice {
public:
    struct TouchPoint_t {
        int num = 0;
        int x = -1;
        int y = -1;
    };

    enum TouchEvent {
        TOUCH_NONE,
        TOUCH_PRESS,
        TOUCH_RELEASE,
        TOUCH_HOLD
    };

    void UpdateTouchPoint()
    {
        ReadRegs(0x02, read_buffer_, 6);
        tp_.num = read_buffer_[0] & 0x0F;
        tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];
        tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];
    }

    TouchEvent CheckTouchEvent()
    {
        bool is_touched = (tp_.num > 0);
        TouchEvent event = TOUCH_NONE;

        if (is_touched && !was_touched_) {
            press_count_++;
            event = TOUCH_PRESS;
            ESP_LOGI("EchoEar", "TOUCH PRESS - count: %d, x: %d, y: %d", press_count_, tp_.x, tp_.y);
        } else if (!is_touched && was_touched_) {
            event = TOUCH_RELEASE;
            ESP_LOGI("EchoEar", "TOUCH RELEASE - total presses: %d", press_count_);
        } else if (is_touched && was_touched_) {
            event = TOUCH_HOLD;
            ESP_LOGD("EchoEar", "TOUCH HOLD - x: %d, y: %d", tp_.x, tp_.y);
        }

        was_touched_ = is_touched;
        return event;
    }
};
```

#### 触摸系统初始化代码
**对应代码** (`EchoEar.cc`):
```cpp
void InitializeCst816sTouchPad()
{
    cst816s_ = new Cst816s(i2c_bus_, 0x15);

    xTaskCreatePinnedToCore(touch_event_task, "touch_task", 4 * 1024, cst816s_, 5, NULL, 1);

    const gpio_config_t int_gpio_config = {
        .pin_bit_mask = (1ULL << TP_PIN_NUM_INT),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&int_gpio_config);
    gpio_install_isr_service(0);
    gpio_intr_enable(TP_PIN_NUM_INT);
    gpio_isr_handler_add(TP_PIN_NUM_INT, EspS3Cat::touch_isr_callback, cst816s_);
}
```

### 4. 电源管理

#### 电源控制
- **主电源控制**: GPIO9
- **编解码器电源**: GPIO48
- **充电管理**: I2C地址0x55

**对应代码** (`config.h`):
```c
#define POWER_CTRL  GPIO_NUM_9
#define CORDEC_POWER_CTRL  GPIO_NUM_48
```

#### 电池监控
- 实时电压监测
- 电流监测
- 温度监测 (ESP32-S3内置温度传感器)

**对应代码** (`EchoEar.cc`):
```cpp
class Charge : public I2cDevice {
public:
    Charge(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr)
    {
        read_buffer_ = new uint8_t[8];
    }

    void Printcharge()
    {
        ReadRegs(0x08, read_buffer_, 2);
        ReadRegs(0x0c, read_buffer_ + 2, 2);
        ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &tsens_value));

        int16_t voltage = static_cast<uint16_t>(read_buffer_[1] << 8 | read_buffer_[0]);
        int16_t current = static_cast<int16_t>(read_buffer_[3] << 8 | read_buffer_[2]);

        // Use the variables to avoid warnings (can be removed if actual implementation uses them)
        (void)voltage;
        (void)current;
    }

    static void TaskFunction(void *pvParameters)
    {
        Charge* charge = static_cast<Charge*>(pvParameters);
        while (true) {
            charge->Printcharge();
            vTaskDelay(pdMS_TO_TICKS(300));
        }
    }
};
```

#### 电源管理初始化代码
**对应代码** (`EchoEar.cc`):
```cpp
void InitializeCharge()
{
    charge_ = new Charge(i2c_bus_, 0x55);
    xTaskCreatePinnedToCore(Charge::TaskFunction, "batterydecTask", 3 * 1024, charge_, 6, NULL, 0);
}

void InitializeButtons()
{
    boot_button_.OnClick([this]() {
        auto &app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
            ESP_LOGI(TAG, "Boot button pressed, enter WiFi configuration mode");
            ResetWifiConfiguration();
        }
        app.ToggleChatState();
    });

    gpio_config_t power_gpio_config = {
        .pin_bit_mask = (BIT64(POWER_CTRL)),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&power_gpio_config));
    gpio_set_level(POWER_CTRL, 0);
}
```

### 5. 用户接口

#### 按键
- **启动按键**: GPIO0
- **音量按键**: 未配置

**对应代码** (`config.h`):
```c
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC
```

#### LED指示
- **绿色LED**: GPIO43
- **内置LED**: 未配置

**对应代码** (`config.h`):
```c
#define LED_G       GPIO_NUM_43
#define BUILTIN_LED_GPIO        GPIO_NUM_NC
```

### 6. 通信接口

#### UART接口
- **UART1 TX**:
  - PCB V1.0: GPIO6
  - PCB V1.2: GPIO5
- **UART1 RX**:
  - PCB V1.0: GPIO5
  - PCB V1.2: GPIO4

**对应代码** (`config.h`):
```c
#define UART1_TX_1     GPIO_NUM_6
#define UART1_TX_2     GPIO_NUM_5
#define UART1_RX_1     GPIO_NUM_5
#define UART1_RX_2     GPIO_NUM_4
```

#### SD卡接口 (预留)
- **MISO**: GPIO17
- **SCK**: GPIO16
- **MOSI**: GPIO38

**对应代码** (`config.h`):
```c
#define SD_MISO     GPIO_NUM_17
#define SD_SCK      GPIO_NUM_16
#define SD_MOSI     GPIO_NUM_38
```

## 软件功能特性

### 1. 显示系统

#### 双UI模式支持
EchoEar支持两种不同的UI显示模式：

##### 自定义表情显示系统 (推荐)
```c
#define USE_LVGL_DEFAULT    0
```
- **特点**: 使用自定义的 `EmoteDisplay` 表情显示系统
- **功能**: 支持丰富的表情动画、眼睛动画、状态图标显示
- **适用**: 智能助手场景，提供更生动的人机交互体验

##### LVGL默认显示系统
```c
#define USE_LVGL_DEFAULT    1
```
- **特点**: 使用标准LVGL图形库的显示系统
- **功能**: 传统的文本和图标显示界面
- **适用**: 需要标准GUI控件的应用场景

#### 表情动画系统
支持多种表情动画：
- **开心** (happy): 快乐表情
- **大笑** (laughing): 享受表情
- **有趣** (funny): 有趣表情
- **爱心** (loving): 爱心表情
- **尴尬** (embarrassed): 尴尬表情
- **自信** (confident): 自信表情
- **美味** (delicious): 美味表情
- **悲伤** (sad): 悲伤表情
- **哭泣** (crying): 哭泣表情
- **困倦** (sleepy): 困倦表情
- **愚蠢** (silly): 愚蠢表情
- **愤怒** (angry): 愤怒表情
- **惊讶** (surprised): 惊讶表情
- **震惊** (shocked): 震惊表情
- **思考** (thinking): 思考表情
- **眨眼** (winking): 眨眼表情
- **放松** (relaxed): 放松表情
- **困惑** (confused): 困惑表情
- **中性** (neutral): 中性表情
- **空闲** (idle): 空闲表情

**对应代码** (`emote_display.cc`):
```cpp
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
        int aaf = std::get<0>(it->second);
        bool repeat = std::get<1>(it->second);
        int fps = std::get<2>(it->second);
        engine_->setEyes(aaf, repeat, fps);
    }
}
```

#### 表情动画资源定义
**对应代码** (`mmap_generate_emoji_normal.h`):
```cpp
enum MMAP_EMOJI_NORMAL_LISTS {
    MMAP_EMOJI_NORMAL_ANGRY_ONE_AAF = 0,        /*!< angry_one.aaf */
    MMAP_EMOJI_NORMAL_DIZZY_ONE_AAF = 1,        /*!< dizzy_one.aaf */
    MMAP_EMOJI_NORMAL_ENJOY_ONE_AAF = 2,        /*!< enjoy_one.aaf */
    MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF = 3,        /*!< happy_one.aaf */
    MMAP_EMOJI_NORMAL_IDLE_ONE_AAF = 4,        /*!< idle_one.aaf */
    MMAP_EMOJI_NORMAL_LISTEN_AAF = 5,        /*!< listen.aaf */
    MMAP_EMOJI_NORMAL_SAD_ONE_AAF = 6,        /*!< sad_one.aaf */
    MMAP_EMOJI_NORMAL_SHOCKED_ONE_AAF = 7,        /*!< shocked_one.aaf */
    MMAP_EMOJI_NORMAL_THINKING_ONE_AAF = 8,        /*!< thinking_one.aaf */
    MMAP_EMOJI_NORMAL_ICON_BATTERY_BIN = 9,        /*!< icon_Battery.bin */
    MMAP_EMOJI_NORMAL_ICON_WIFI_FAILED_BIN = 10,        /*!< icon_WiFi_failed.bin */
    MMAP_EMOJI_NORMAL_ICON_MIC_BIN = 11,        /*!< icon_mic.bin */
    MMAP_EMOJI_NORMAL_ICON_SPEAKER_ZZZ_BIN = 12,        /*!< icon_speaker_zzz.bin */
    MMAP_EMOJI_NORMAL_ICON_WIFI_BIN = 13,        /*!< icon_wifi.bin */
    MMAP_EMOJI_NORMAL_KAITI_TTF = 14,        /*!< KaiTi.ttf */
};
```

#### 状态图标系统
- **电池图标**: 显示电池状态
- **WiFi图标**: 显示网络连接状态
- **麦克风图标**: 显示录音状态
- **扬声器图标**: 显示播放状态
- **错误图标**: 显示错误状态

**对应代码** (`emote_display.cc`):
```cpp
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
```

#### 文本显示功能
- **提示文本**: 显示系统提示信息
- **时间显示**: 显示当前时间 (HH:MM格式)
- **聊天消息**: 显示对话内容
- **滚动文本**: 支持长文本滚动显示

**对应代码** (`emote_display.cc`):
```cpp
void EmoteDisplay::SetChatMessage(const char* role, const char* content)
{
    engine_->Lock();
    if (content && strlen(content) > 0) {
        gfx_label_set_text(obj_label_tips, content);
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);
    }
    engine_->Unlock();
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
```

### 2. 音频系统

#### 音频编解码
- **录音**: ES7210 ADC，支持双麦阵列
- **播放**: ES8311 DAC，支持功放输出
- **采样率**: 24kHz
- **位深度**: 16位

**对应代码** (`config.h`):
```c
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_INPUT_REFERENCE    true
```

#### 音频处理
- **声源定位**: 支持双麦声源定位算法
- **语音唤醒**: 支持离线语音唤醒
- **音频增强**: 支持音频预处理和后处理

**对应代码** (`EchoEar.cc`):
```cpp
virtual AudioCodec* GetAudioCodec() override
{
    static BoxAudioCodec audio_codec(
        i2c_bus_,
        AUDIO_INPUT_SAMPLE_RATE,
        AUDIO_OUTPUT_SAMPLE_RATE,
        AUDIO_I2S_GPIO_MCLK,
        AUDIO_I2S_GPIO_BCLK,
        AUDIO_I2S_GPIO_WS,
        AUDIO_I2S_GPIO_DOUT,
        AUDIO_I2S_GPIO_DIN,
        AUDIO_CODEC_PA_PIN,
        AUDIO_CODEC_ES8311_ADDR,
        AUDIO_CODEC_ES7210_ADDR,
        AUDIO_INPUT_REFERENCE);
    return &audio_codec;
}
```

### 3. 触摸交互

#### 触摸事件处理
- **单击**: 切换聊天状态
- **长按**: 持续触摸检测
- **WiFi配置**: 启动时触摸进入WiFi配置模式

**对应代码** (`EchoEar.cc`):
```cpp
static void touch_event_task(void* arg)
{
    Cst816s* touchpad = static_cast<Cst816s*>(arg);
    if (touchpad == nullptr) {
        ESP_LOGE(TAG, "Invalid touchpad pointer in touch_event_task");
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        if (touchpad->WaitForTouchEvent()) {
            auto &app = Application::GetInstance();
            auto &board = (EspS3Cat &)Board::GetInstance();

            ESP_LOGI(TAG, "Touch event, TP_PIN_NUM_INT: %d", gpio_get_level(TP_PIN_NUM_INT));
            touchpad->UpdateTouchPoint();
            auto touch_event = touchpad->CheckTouchEvent();

            if (touch_event == Cst816s::TOUCH_RELEASE) {
                if (app.GetDeviceState() == kDeviceStateStarting &&
                        !WifiStation::GetInstance().IsConnected()) {
                    board.ResetWifiConfiguration();
                } else {
                    app.ToggleChatState();
                }
            }
        }
    }
}
```

#### 触摸反馈
- 实时触摸坐标检测
- 触摸事件计数
- 触摸状态变化通知

**对应代码** (`EchoEar.cc`):
```cpp
static void touch_isr_callback(void* arg)
{
    Cst816s* touchpad = static_cast<Cst816s*>(arg);
    if (touchpad != nullptr) {
        touchpad->NotifyTouchEvent();
    }
}
```

### 4. 电源管理

#### 电池监控
- 实时电压监测 (通过I2C读取充电IC)
- 电流监测
- 温度监测
- 充电状态检测

**对应代码** (`EchoEar.cc`):
```cpp
class Charge : public I2cDevice {
public:
    void Printcharge()
    {
        ReadRegs(0x08, read_buffer_, 2);
        ReadRegs(0x0c, read_buffer_ + 2, 2);
        ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &tsens_value));

        int16_t voltage = static_cast<uint16_t>(read_buffer_[1] << 8 | read_buffer_[0]);
        int16_t current = static_cast<int16_t>(read_buffer_[3] << 8 | read_buffer_[2]);

        // Use the variables to avoid warnings (can be removed if actual implementation uses them)
        (void)voltage;
        (void)current;
    }

    static void TaskFunction(void *pvParameters)
    {
        Charge* charge = static_cast<Charge*>(pvParameters);
        while (true) {
            charge->Printcharge();
            vTaskDelay(pdMS_TO_TICKS(300));
        }
    }
};
```

#### 电源控制
- 主电源开关控制
- 编解码器电源管理
- 背光亮度控制

**对应代码** (`EchoEar.cc`):
```cpp
void InitializeButtons()
{
    boot_button_.OnClick([this]() {
        auto &app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
            ESP_LOGI(TAG, "Boot button pressed, enter WiFi configuration mode");
            ResetWifiConfiguration();
        }
        app.ToggleChatState();
    });

    gpio_config_t power_gpio_config = {
        .pin_bit_mask = (BIT64(POWER_CTRL)),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&power_gpio_config));
    gpio_set_level(POWER_CTRL, 0);
}
```

### 5. 网络连接

#### WiFi功能
- WiFi Station模式
- 自动连接配置
- 连接状态监控
- WiFi配置重置功能

**对应代码** (`EchoEar.cc`):
```cpp
// WiFi配置重置功能
if (app.GetDeviceState() == kDeviceStateStarting &&
        !WifiStation::GetInstance().IsConnected()) {
    board.ResetWifiConfiguration();
}
```

#### 4G模块支持 (可选)
- 支持4G模块扩展
- 通过UART通信

**对应代码** (`config.h`):
```c
#define UART1_TX_1     GPIO_NUM_6
#define UART1_TX_2     GPIO_NUM_5
#define UART1_RX_1     GPIO_NUM_5
#define UART1_RX_2     GPIO_NUM_4
```

## 操作指南

### 1. 设备启动

#### 正常启动流程
1. 设备上电后自动初始化
2. 检测PCB版本 (V1.0或V1.2)
3. 初始化I2C总线
4. 初始化音频系统
5. 初始化显示系统
6. 初始化触摸系统
7. 启动WiFi连接
8. 显示启动界面

**对应代码** (`EchoEar.cc`):
```cpp
EspS3Cat() : boot_button_(BOOT_BUTTON_GPIO)
{
    InitializeI2c();
    uint8_t pcb_verison = DetectPcbVersion();
    InitializeCharge();
    InitializeCst816sTouchPad();

    InitializeSpi();
    Initializest77916Display(pcb_verison);
    InitializeButtons();
}

void InitializeI2c()
{
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
}
```

#### 启动状态指示
- **启动中**: 显示"启动中..."文本
- **WiFi连接中**: 显示WiFi图标
- **连接失败**: 显示错误图标
- **待命状态**: 显示电池图标和时间

**对应代码** (`emote_display.cc`):
```cpp
// 初始化时设置启动状态
current_icon_type = MMAP_EMOJI_NORMAL_ICON_WIFI_FAILED_BIN;
SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);

// 状态切换逻辑
void EmoteDisplay::SetStatus(const char* status)
{
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
}
```

### 2. 用户交互

#### 触摸操作
- **单击屏幕**: 切换聊天状态
- **启动时触摸**: 进入WiFi配置模式

**对应代码** (`EchoEar.cc`):
```cpp
// 触摸事件处理
if (touch_event == Cst816s::TOUCH_RELEASE) {
    if (app.GetDeviceState() == kDeviceStateStarting &&
            !WifiStation::GetInstance().IsConnected()) {
        board.ResetWifiConfiguration();
    } else {
        app.ToggleChatState();
    }
}
```

#### 按键操作
- **启动按键**: 切换聊天状态或进入WiFi配置

**对应代码** (`EchoEar.cc`):
```cpp
boot_button_.OnClick([this]() {
    auto &app = Application::GetInstance();
    if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
        ESP_LOGI(TAG, "Boot button pressed, enter WiFi configuration mode");
        ResetWifiConfiguration();
    }
    app.ToggleChatState();
});
```

#### 语音交互
- **语音唤醒**: 说出唤醒词激活设备
- **语音指令**: 通过语音与AI助手交互
- **语音反馈**: 设备通过语音回复

**对应代码** (`EchoEar.cc`):
```cpp
// 音频编解码器获取
virtual AudioCodec* GetAudioCodec() override
{
    static BoxAudioCodec audio_codec(
        i2c_bus_,
        AUDIO_INPUT_SAMPLE_RATE,
        AUDIO_OUTPUT_SAMPLE_RATE,
        AUDIO_I2S_GPIO_MCLK,
        AUDIO_I2S_GPIO_BCLK,
        AUDIO_I2S_GPIO_WS,
        AUDIO_I2S_GPIO_DOUT,
        AUDIO_I2S_GPIO_DIN,
        AUDIO_CODEC_PA_PIN,
        AUDIO_CODEC_ES8311_ADDR,
        AUDIO_CODEC_ES7210_ADDR,
        AUDIO_INPUT_REFERENCE);
    return &audio_codec;
}
```

### 3. 状态管理

#### 设备状态
- **启动中** (kDeviceStateStarting): 设备初始化阶段
- **待命** (kDeviceStateIdle): 设备空闲状态
- **聆听中**: 设备正在录音
- **说话中**: 设备正在播放音频
- **错误**: 设备出现错误

**对应代码** (`EchoEar.cc`):
```cpp
// 设备状态检查
if (app.GetDeviceState() == kDeviceStateStarting &&
        !WifiStation::GetInstance().IsConnected()) {
    board.ResetWifiConfiguration();
} else {
    app.ToggleChatState();
}
```

#### 显示状态切换
- **待命状态**: 显示时间 + 电池图标
- **聆听状态**: 显示麦克风动画 + 麦克风图标
- **说话状态**: 显示文本内容 + 扬声器图标
- **错误状态**: 显示错误信息 + 错误图标

**对应代码** (`emote_display.cc`):
```cpp
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
```

### 4. 配置管理

#### WiFi配置
- **自动配置**: 设备启动时自动连接已保存的WiFi
- **手动配置**: 通过触摸或按键进入配置模式
- **配置重置**: 清除WiFi配置信息

**对应代码** (`EchoEar.cc`):
```cpp
// WiFi配置重置
if (app.GetDeviceState() == kDeviceStateStarting &&
        !WifiStation::GetInstance().IsConnected()) {
    board.ResetWifiConfiguration();
}
```

#### 系统配置
- **UI模式选择**: 通过修改代码宏定义选择UI模式
- **分区表配置**: 使用16MB Flash专用分区表
- **音频参数**: 24kHz采样率配置

**对应代码** (`EchoEar.cc`):
```cpp
#define USE_LVGL_DEFAULT    0  // 使用表情显示系统
#define USE_LVGL_DEFAULT    1  // 使用LVGL显示系统

// 分区表配置
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v1/16m_echoear.csv\""

// 音频参数配置
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
```

## 开发指南

### 1. 编译配置

#### 目标配置
```bash
idf.py set-target esp32s3
```

#### 菜单配置
```bash
idf.py menuconfig
```

#### 必要配置项
- **Board Type**: 选择 `EchoEar`
- **Partition Table**: 选择 `Custom partition table CSV`
- **Custom partition CSV file**: 输入 `partitions/v1/16m_echoear.csv`

### 2. 代码修改

#### UI模式切换
修改 `main/boards/echoear/EchoEar.cc` 第29行：
```c
#define USE_LVGL_DEFAULT    0  // 使用表情显示系统
#define USE_LVGL_DEFAULT    1  // 使用LVGL显示系统
```

**对应代码** (`EchoEar.cc`):
```cpp
#define USE_LVGL_DEFAULT    0

#if USE_LVGL_DEFAULT
    display_ = new SpiLcdDisplay(panel_io, panel,
        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
#else
    display_ = new anim::EmoteDisplay(panel, panel_io);
#endif
```

#### 表情动画添加
在 `emote_display.cc` 的 `emotion_map` 中添加新的表情映射。

**对应代码** (`emote_display.cc`):
```cpp
static const std::unordered_map<std::string, EmotionParam> emotion_map = {
    {"happy",       {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
    {"laughing",    {MMAP_EMOJI_NORMAL_ENJOY_ONE_AAF,     true,  20}},
    // 添加新表情
    {"new_emotion", {MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF,     true,  20}},
    // ...
};
```

#### 状态处理
在 `SetStatus` 函数中添加新的状态处理逻辑。

**对应代码** (`emote_display.cc`):
```cpp
void EmoteDisplay::SetStatus(const char* status)
{
    if (std::strcmp(status, "聆听中...") == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_ANIM_TOP);
        engine_->setEyes(MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF, true, 20);
        engine_->SetIcon(MMAP_EMOJI_NORMAL_ICON_MIC_BIN);
    } else if (std::strcmp(status, "新状态") == 0) {
        // 添加新状态处理
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);
        engine_->SetIcon(MMAP_EMOJI_NORMAL_ICON_WIFI_BIN);
    }
    // ...
}
```

### 3. 资源管理

#### 资源文件
- **表情动画**: AAF格式动画文件
- **图标资源**: BIN格式图标文件
- **字体文件**: TTF格式字体文件
- **音频资源**: OGG格式音频文件

**对应代码** (`mmap_generate_emoji_normal.h`):
```cpp
enum MMAP_EMOJI_NORMAL_LISTS {
    MMAP_EMOJI_NORMAL_ANGRY_ONE_AAF = 0,        /*!< angry_one.aaf */
    MMAP_EMOJI_NORMAL_DIZZY_ONE_AAF = 1,        /*!< dizzy_one.aaf */
    MMAP_EMOJI_NORMAL_ENJOY_ONE_AAF = 2,        /*!< enjoy_one.aaf */
    MMAP_EMOJI_NORMAL_HAPPY_ONE_AAF = 3,        /*!< happy_one.aaf */
    MMAP_EMOJI_NORMAL_IDLE_ONE_AAF = 4,        /*!< idle_one.aaf */
    MMAP_EMOJI_NORMAL_LISTEN_AAF = 5,        /*!< listen.aaf */
    MMAP_EMOJI_NORMAL_SAD_ONE_AAF = 6,        /*!< sad_one.aaf */
    MMAP_EMOJI_NORMAL_SHOCKED_ONE_AAF = 7,        /*!< shocked_one.aaf */
    MMAP_EMOJI_NORMAL_THINKING_ONE_AAF = 8,        /*!< thinking_one.aaf */
    MMAP_EMOJI_NORMAL_ICON_BATTERY_BIN = 9,        /*!< icon_Battery.bin */
    MMAP_EMOJI_NORMAL_ICON_WIFI_FAILED_BIN = 10,        /*!< icon_WiFi_failed.bin */
    MMAP_EMOJI_NORMAL_ICON_MIC_BIN = 11,        /*!< icon_mic.bin */
    MMAP_EMOJI_NORMAL_ICON_SPEAKER_ZZZ_BIN = 12,        /*!< icon_speaker_zzz.bin */
    MMAP_EMOJI_NORMAL_ICON_WIFI_BIN = 13,        /*!< icon_wifi.bin */
    MMAP_EMOJI_NORMAL_KAITI_TTF = 14,        /*!< KaiTi.ttf */
};
```

#### 内存映射
- 使用 `mmap_assets` 系统管理资源文件
- 支持内存映射访问，提高访问效率
- 资源文件存储在 `assets_A` 分区

**对应代码** (`emote_display.cc`):
```cpp
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
```

### 4. 调试功能

#### 日志输出
- 使用ESP_LOGI/ESP_LOGE等宏输出调试信息
- 标签分类：EchoEar、emoji等

**对应代码** (`EchoEar.cc`):
```cpp
#define TAG "EchoEar"

ESP_LOGI(TAG, "PCB verison V1.0");
ESP_LOGI(TAG, "PCB verison V1.2");
ESP_LOGE(TAG, "PCB version detection error");
ESP_LOGI(TAG, "Boot button pressed, enter WiFi configuration mode");
ESP_LOGI(TAG, "Touch event, TP_PIN_NUM_INT: %d", gpio_get_level(TP_PIN_NUM_INT));
```

#### 触摸调试
- 触摸坐标实时输出
- 触摸事件计数
- 触摸状态变化日志

**对应代码** (`EchoEar.cc`):
```cpp
ESP_LOGI("EchoEar", "TOUCH PRESS - count: %d, x: %d, y: %d", press_count_, tp_.x, tp_.y);
ESP_LOGI("EchoEar", "TOUCH RELEASE - total presses: %d", press_count_);
ESP_LOGD("EchoEar", "TOUCH HOLD - x: %d, y: %d", tp_.x, tp_.y);
```

#### 音频调试
- 音频参数监控
- 编解码器状态检查
- 音频数据流监控

**对应代码** (`EchoEar.cc`):
```cpp
// 电池和温度监控
ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &tsens_value));
int16_t voltage = static_cast<uint16_t>(read_buffer_[1] << 8 | read_buffer_[0]);
int16_t current = static_cast<int16_t>(read_buffer_[3] << 8 | read_buffer_[2]);
```

## 故障排除

### 1. 常见问题

#### 显示问题
- **屏幕不亮**: 检查背光控制引脚配置
- **显示异常**: 检查QSPI接口连接
- **触摸无响应**: 检查触摸控制器I2C连接

#### 音频问题
- **无声音输出**: 检查功放控制引脚
- **录音异常**: 检查麦克风连接
- **音质问题**: 检查I2S接口配置

#### 网络问题
- **WiFi连接失败**: 检查WiFi配置
- **网络不稳定**: 检查天线连接

### 2. 硬件检查

#### 电源检查
- 检查电池电压
- 检查电源控制引脚
- 检查充电功能

#### 接口检查
- 检查I2C总线连接
- 检查SPI接口连接
- 检查GPIO配置

### 3. 软件调试

#### 日志分析
- 查看启动日志
- 分析错误信息
- 检查配置参数

#### 功能测试
- 测试触摸功能
- 测试音频功能
- 测试显示功能

## 技术规格

### 1. 电气特性
- **工作电压**: 3.3V
- **工作电流**: 待机 < 50mA，工作 < 200mA
- **工作温度**: -10°C ~ +60°C
- **存储温度**: -20°C ~ +70°C

### 2. 接口规格
- **I2C**: 标准I2C接口，支持多设备
- **SPI**: QSPI接口，支持高速数据传输
- **UART**: 标准UART接口，支持调试和扩展
- **GPIO**: 多路GPIO，支持各种外设连接

### 3. 性能指标
- **显示刷新率**: 30fps
- **触摸响应时间**: < 50ms
- **音频延迟**: < 100ms
- **启动时间**: < 5秒

## 总结

EchoEar 喵伴是一款功能丰富的智能AI开发套件，集成了先进的显示、音频、触摸和电源管理功能。通过本手册，您可以全面了解设备的硬件配置、软件功能和操作方法，为开发和调试提供完整的参考。

设备支持两种UI模式，可以根据应用需求选择合适的显示系统。丰富的表情动画和状态指示功能，为智能助手应用提供了生动的人机交互体验。

通过合理的配置和开发，EchoEar可以广泛应用于智能家居、教育机器人、语音助手等各种AI应用场景。
