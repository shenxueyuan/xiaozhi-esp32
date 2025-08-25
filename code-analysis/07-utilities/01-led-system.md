# LED系统 (Led) 详解

## 📁 模块概览
- **文件数量**: 7个文件 (4个.h + 3个.cc)
- **功能**: LED状态指示，支持多种LED类型
- **设计模式**: 策略模式 + 状态模式
- **支持类型**: GPIO LED、环形LED灯带、单色LED

## 🏗️ 架构设计

### 继承层次结构
```
Led (抽象基类)
├── GpioLed              # 简单GPIO控制LED
├── CircularStrip        # 环形LED灯带
├── SingleLed            # 单个LED控制
└── NoLed                # 无LED实现
```

## 🎯 Led基类分析

### 极简接口设计
```cpp
class Led {
public:
    virtual ~Led() = default;

    // 根据设备状态设置LED状态
    virtual void OnStateChanged() = 0;
};

class NoLed : public Led {
public:
    virtual void OnStateChanged() override {}
};
```

### 设计理念
LED系统采用极简化设计，只有一个核心方法：
- **状态驱动**: LED状态完全由设备状态决定
- **自动响应**: 设备状态变化时自动调用`OnStateChanged()`
- **解耦设计**: LED实现不需要了解具体的设备状态细节
- **扩展友好**: 新的LED类型只需实现状态变化响应

## 💡 GpioLed 实现分析

### 简单GPIO LED控制
```cpp
class GpioLed : public Led {
private:
    gpio_num_t led_pin_;              // LED GPIO引脚
    bool active_high_;                // 高电平有效标志
    bool current_state_;              // 当前LED状态
    esp_timer_handle_t blink_timer_;  // 闪烁定时器
    bool blinking_;                   // 是否正在闪烁
    int blink_interval_ms_;           // 闪烁间隔

public:
    GpioLed(gpio_num_t pin, bool active_high = true);
    virtual ~GpioLed();

    virtual void OnStateChanged() override;

    // LED控制方法
    void TurnOn();
    void TurnOff();
    void Toggle();
    void StartBlinking(int interval_ms = 500);
    void StopBlinking();
    void SetBrightness(uint8_t brightness);  // PWM调光

private:
    void InitializeGpio();
    void SetLedState(bool on);
    static void BlinkTimerCallback(void* arg);
    DeviceState GetCurrentDeviceState();
};
```

### GPIO初始化和控制
```cpp
GpioLed::GpioLed(gpio_num_t pin, bool active_high)
    : led_pin_(pin), active_high_(active_high), current_state_(false),
      blink_timer_(nullptr), blinking_(false), blink_interval_ms_(500) {
    InitializeGpio();
}

void GpioLed::InitializeGpio() {
    // 配置GPIO为输出模式
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << led_pin_),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&config));

    // 初始状态为关闭
    SetLedState(false);
}

void GpioLed::SetLedState(bool on) {
    current_state_ = on;

    // 根据active_high_决定实际输出电平
    int level = active_high_ ? (on ? 1 : 0) : (on ? 0 : 1);
    gpio_set_level(led_pin_, level);
}
```

### 状态响应实现
```cpp
void GpioLed::OnStateChanged() {
    DeviceState state = GetCurrentDeviceState();

    switch (state) {
        case kDeviceStateStarting:
            StartBlinking(200);  // 快速闪烁 - 启动中
            break;

        case kDeviceStateIdle:
            StopBlinking();
            TurnOn();            // 常亮 - 待机
            break;

        case kDeviceStateListening:
            StartBlinking(500);  // 中速闪烁 - 聆听中
            break;

        case kDeviceStatePlaying:
            StopBlinking();
            TurnOn();            // 常亮 - 播放中
            break;

        case kDeviceStateError:
            StartBlinking(100);  // 急速闪烁 - 错误状态
            break;

        default:
            TurnOff();           // 关闭 - 未知状态
            break;
    }
}
```

### PWM调光功能
```cpp
void GpioLed::SetBrightness(uint8_t brightness) {
    // 配置LEDC PWM
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,  // 1kHz PWM频率
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t channel_conf = {
        .gpio_num = led_pin_,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = brightness,  // 0-255
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));
}
```

## 🌈 CircularStrip 环形LED灯带分析

### WS2812B LED灯带控制
```cpp
class CircularStrip : public Led {
private:
    gpio_num_t data_pin_;             // 数据引脚
    int led_count_;                   // LED数量
    rmt_channel_handle_t rmt_channel_; // RMT通道句柄
    rmt_encoder_handle_t led_encoder_; // LED编码器
    uint8_t* led_buffer_;             // LED颜色缓冲区

    // 动画参数
    esp_timer_handle_t animation_timer_;
    int animation_speed_;
    int animation_step_;
    AnimationType current_animation_;

    // 颜色定义
    struct Color {
        uint8_t r, g, b;
        Color(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0)
            : r(red), g(green), b(blue) {}
    };

public:
    CircularStrip(gpio_num_t pin, int led_count);
    virtual ~CircularStrip();

    virtual void OnStateChanged() override;

    // LED灯带控制方法
    void SetColor(int led_index, Color color);
    void SetAllColor(Color color);
    void ClearAll();
    void UpdateDisplay();

    // 动画效果
    void StartBreathingAnimation(Color color, int speed = 100);
    void StartRotatingAnimation(Color color, int speed = 50);
    void StartRainbowAnimation(int speed = 30);
    void StartWaveAnimation(Color color, int speed = 80);
    void StopAnimation();

private:
    void InitializeRmt();
    void InitializeLedEncoder();
    static void AnimationTimerCallback(void* arg);
    void ProcessBreathingAnimation();
    void ProcessRotatingAnimation();
    void ProcessRainbowAnimation();
    void ProcessWaveAnimation();
    DeviceState GetCurrentDeviceState();
};
```

### RMT驱动初始化
```cpp
void CircularStrip::InitializeRmt() {
    // 配置RMT发送通道
    rmt_tx_channel_config_t tx_config = {
        .gpio_num = data_pin_,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,  // 10MHz分辨率
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags = {
            .invert_out = false,
            .with_dma = false,
        }
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_config, &rmt_channel_));
    ESP_ERROR_CHECK(rmt_enable(rmt_channel_));
}

void CircularStrip::InitializeLedEncoder() {
    // WS2812编码器配置
    led_strip_encoder_config_t encoder_config = {
        .resolution = 10000000,  // 10MHz
    };

    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder_));
}
```

### 状态指示动画
```cpp
void CircularStrip::OnStateChanged() {
    DeviceState state = GetCurrentDeviceState();

    switch (state) {
        case kDeviceStateStarting:
            // 旋转蓝色光圈
            StartRotatingAnimation(Color(0, 0, 255), 100);
            break;

        case kDeviceStateIdle:
            // 呼吸绿色
            StartBreathingAnimation(Color(0, 255, 0), 150);
            break;

        case kDeviceStateListening:
            // 波浪白色
            StartWaveAnimation(Color(255, 255, 255), 80);
            break;

        case kDeviceStatePlaying:
            // 静态橙色
            StopAnimation();
            SetAllColor(Color(255, 165, 0));
            UpdateDisplay();
            break;

        case kDeviceStateError:
            // 闪烁红色
            StartBreathingAnimation(Color(255, 0, 0), 50);
            break;

        default:
            StopAnimation();
            ClearAll();
            UpdateDisplay();
            break;
    }
}
```

### 彩虹动画实现
```cpp
void CircularStrip::ProcessRainbowAnimation() {
    for (int i = 0; i < led_count_; ++i) {
        // HSV到RGB转换实现彩虹效果
        float hue = fmod((animation_step_ * 2.0f + i * 360.0f / led_count_), 360.0f);
        Color color = HsvToRgb(hue, 1.0f, 1.0f);
        SetColor(i, color);
    }

    UpdateDisplay();
    animation_step_++;

    if (animation_step_ >= 180) {
        animation_step_ = 0;  // 重置动画步骤
    }
}

CircularStrip::Color CircularStrip::HsvToRgb(float h, float s, float v) {
    float c = v * s;
    float x = c * (1 - abs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;

    float r, g, b;
    if (h < 60) {
        r = c; g = x; b = 0;
    } else if (h < 120) {
        r = x; g = c; b = 0;
    } else if (h < 180) {
        r = 0; g = c; b = x;
    } else if (h < 240) {
        r = 0; g = x; b = c;
    } else if (h < 300) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }

    return Color((r + m) * 255, (g + m) * 255, (b + m) * 255);
}
```

## 🔆 SingleLed 分析

### 单LED精确控制
```cpp
class SingleLed : public Led {
private:
    gpio_num_t led_pin_;
    bool current_state_;
    uint32_t blink_pattern_;          // 32位闪烁模式
    int pattern_position_;            // 当前模式位置
    esp_timer_handle_t pattern_timer_;

public:
    SingleLed(gpio_num_t pin);

    virtual void OnStateChanged() override;

    // 模式控制
    void SetBlinkPattern(uint32_t pattern, int interval_ms = 100);
    void SetCustomPattern(const std::vector<int>& durations);

private:
    void ProcessBlinkPattern();
    static void PatternTimerCallback(void* arg);
};
```

### 闪烁模式定义
```cpp
// 预定义闪烁模式 (1为亮，0为暗)
static constexpr uint32_t PATTERN_SOLID_ON    = 0xFFFFFFFF;  // 常亮
static constexpr uint32_t PATTERN_SOLID_OFF   = 0x00000000;  // 常暗
static constexpr uint32_t PATTERN_SLOW_BLINK  = 0xF0F0F0F0;  // 慢闪
static constexpr uint32_t PATTERN_FAST_BLINK  = 0xAAAAAAAA;  // 快闪
static constexpr uint32_t PATTERN_HEARTBEAT   = 0xCC0CC000;  // 心跳
static constexpr uint32_t PATTERN_SOS         = 0x15D77571;  // SOS摩尔斯码

void SingleLed::OnStateChanged() {
    DeviceState state = GetCurrentDeviceState();

    switch (state) {
        case kDeviceStateStarting:
            SetBlinkPattern(PATTERN_FAST_BLINK, 100);
            break;

        case kDeviceStateIdle:
            SetBlinkPattern(PATTERN_SOLID_ON);
            break;

        case kDeviceStateListening:
            SetBlinkPattern(PATTERN_HEARTBEAT, 200);
            break;

        case kDeviceStatePlaying:
            SetBlinkPattern(PATTERN_SLOW_BLINK, 500);
            break;

        case kDeviceStateError:
            SetBlinkPattern(PATTERN_SOS, 150);
            break;

        default:
            SetBlinkPattern(PATTERN_SOLID_OFF);
            break;
    }
}
```

## 🔧 LED系统集成

### Board集成
```cpp
class Board {
public:
    virtual Led* GetLed() {
        static NoLed led;  // 默认无LED实现
        return &led;
    }
};

// 具体开发板实现
class EspSparkBot : public WifiBoard {
public:
    virtual Led* GetLed() override {
        static CircularStrip led(LED_DATA_PIN, 12);  // 12个LED的环形灯带
        return &led;
    }
};

class AtomMatrix : public WifiBoard {
public:
    virtual Led* GetLed() override {
        static GpioLed led(BUILTIN_LED_GPIO);  // 内置GPIO LED
        return &led;
    }
};
```

### Application集成
```cpp
void Application::OnDeviceStateChanged(DeviceState new_state) {
    device_state_ = new_state;

    // 通知LED系统状态变化
    auto* led = Board::GetInstance().GetLed();
    if (led) {
        led->OnStateChanged();
    }

    // 通知其他组件
    auto* display = Board::GetInstance().GetDisplay();
    if (display) {
        display->UpdateStatusBar();
    }
}
```

### 状态管理优化
```cpp
class LedStateManager {
private:
    Led* led_;
    DeviceState last_state_;
    std::chrono::steady_clock::time_point last_update_;

public:
    LedStateManager(Led* led) : led_(led), last_state_(kDeviceStateIdle) {}

    void UpdateState(DeviceState new_state) {
        // 防抖动 - 避免频繁状态切换
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_update_
        ).count();

        if (new_state != last_state_ && duration > 100) {  // 100ms防抖
            last_state_ = new_state;
            last_update_ = now;

            if (led_) {
                led_->OnStateChanged();
            }
        }
    }
};
```

## ⚡ 性能优化

### 内存优化
```cpp
// 静态分配LED缓冲区
static uint8_t s_led_buffer[LED_COUNT * 3];  // RGB数据

// 避免动态分配
void CircularStrip::SetColor(int led_index, Color color) {
    if (led_index >= 0 && led_index < led_count_) {
        int offset = led_index * 3;
        led_buffer_[offset + 0] = color.g;  // WS2812B: GRB序列
        led_buffer_[offset + 1] = color.r;
        led_buffer_[offset + 2] = color.b;
    }
}
```

### CPU优化
```cpp
// 高效的动画处理
void CircularStrip::ProcessRotatingAnimation() {
    // 使用位移操作优化旋转计算
    int shift = animation_step_ % led_count_;

    for (int i = 0; i < led_count_; ++i) {
        int led_index = (i + shift) % led_count_;

        // 预计算亮度衰减
        uint8_t brightness = (i < 3) ? (255 >> i) : 0;
        Color color(brightness, brightness, brightness);

        SetColor(led_index, color);
    }

    UpdateDisplay();
    animation_step_++;
}
```

### 功耗控制
```cpp
void CircularStrip::SetPowerSaveMode(bool enable) {
    if (enable) {
        // 降低LED亮度节省功耗
        for (int i = 0; i < led_count_ * 3; ++i) {
            led_buffer_[i] = led_buffer_[i] >> 2;  // 亮度降为1/4
        }

        // 降低动画刷新率
        animation_speed_ = animation_speed_ * 2;
    } else {
        // 恢复正常亮度和速度
        RestoreNormalMode();
    }

    UpdateDisplay();
}
```

## 🎨 自定义LED效果

### 音乐可视化
```cpp
class MusicVisualizationLed : public CircularStrip {
private:
    std::vector<float> audio_spectrum_;  // 音频频谱数据

public:
    void UpdateAudioSpectrum(const std::vector<float>& spectrum) {
        audio_spectrum_ = spectrum;

        // 将频谱映射到LED颜色
        for (int i = 0; i < led_count_; ++i) {
            int spectrum_index = i * spectrum.size() / led_count_;
            float amplitude = spectrum[spectrum_index];

            // 振幅映射到颜色亮度
            uint8_t brightness = static_cast<uint8_t>(amplitude * 255);
            Color color = AmplitudeToColor(amplitude);
            color.r = (color.r * brightness) / 255;
            color.g = (color.g * brightness) / 255;
            color.b = (color.b * brightness) / 255;

            SetColor(i, color);
        }

        UpdateDisplay();
    }

private:
    Color AmplitudeToColor(float amplitude) {
        // 频谱幅度映射到颜色
        if (amplitude < 0.3f) {
            return Color(0, 255, 0);      // 绿色 - 低频
        } else if (amplitude < 0.7f) {
            return Color(255, 255, 0);    // 黄色 - 中频
        } else {
            return Color(255, 0, 0);      // 红色 - 高频
        }
    }
};
```

---

**相关文档**:
- [Board硬件抽象层](../05-board-abstraction/01-board-base.md)
- [应用状态管理](../02-main-core/02-application-class.md)
- [GPIO和外设配置](../05-board-abstraction/02-board-implementations.md)
