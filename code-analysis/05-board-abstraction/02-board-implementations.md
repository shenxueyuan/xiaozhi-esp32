# 开发板具体实现详解

## 📁 模块概览
- **开发板总数**: 68个不同的开发板实现
- **分类**: 官方板卡、第三方厂商、定制板卡
- **特色**: 每个板卡都有独特的硬件配置和功能特性

## 🏗️ 分类架构

### 按厂商分类
```
官方开发板 (8个)
├── esp-box系列 (4个)          # 智能音箱类型
├── esp-sparkbot              # 机器人开发板
├── esp-hi                    # 小巧紧凑型
├── esp-s3-lcd-ev-board系列   # LCD评估板
└── esp32s3-korvo2-v3        # 语音专用板

第三方厂商 (60个)
├── M5Stack生态 (6个)         # M5Stack系列
├── LilyGo系列 (4个)          # TTGO/LilyGo
├── WaveShare系列 (7个)       # 微雪电子
├── 正点原子系列 (8个)         # ATK开发板
├── 其他厂商 (35个)           # 各种定制板卡
```

### 按功能分类
```
智能音箱类型
├── esp-box, esp-box-3, esp-box-lite
├── atoms3-echo-base, atoms3r-echo-base
└── sensecap-watcher

机器人/移动类型
├── esp-sparkbot, otto-robot, electron-bot
└── 各种小车底盘板卡

显示屏开发板
├── 触摸LCD系列 (多个尺寸)
├── AMOLED系列
└── 评估板系列

紧凑型开发板
├── esp-hi, bread-compact系列
├── 各种C3/C6小型板卡
└── 面包板兼容版本
```

## 🎯 代表性开发板详细分析

### 1. ESP-SparkBot (机器人开发板)

#### 硬件配置特点
```cpp
// 音频配置 - 16kHz采样率，适合语音识别
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000

// I2S音频引脚配置
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_45    // 主时钟
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_41    // 字选择
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_39    // 位时钟
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_40    // 数据输入(录音)
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_42    // 数据输出(播放)

// 音频编解码器 - ES8311
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_46  // 功放控制
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_4   // I2C数据
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_5   // I2C时钟

// 显示屏配置 - 240x240 TFT LCD
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_DC_GPIO     GPIO_NUM_43     // 数据/命令控制
#define DISPLAY_CS_GPIO     GPIO_NUM_44     // 片选
#define DISPLAY_CLK_GPIO    GPIO_NUM_21     // SPI时钟
#define DISPLAY_MOSI_GPIO   GPIO_NUM_47     // SPI数据

// 摄像头配置 - OV3660
#define SPARKBOT_CAMERA_XCLK    GPIO_NUM_15  // 摄像头时钟
#define SPARKBOT_CAMERA_PCLK    GPIO_NUM_13  // 像素时钟
// ... 8位数据线配置
```

#### 特色功能实现
```cpp
class EspSparkBot : public WifiBoard {
private:
    // 机器人特有的灯光模式
    light_mode_t light_mode_ = LIGHT_MODE_ALWAYS_ON;

    // UART回声基座通信 (与机器人底盘通信)
    void InitializeEchoUart() {
        uart_config_t uart_config = {
            .baud_rate = ECHO_UART_BAUD_RATE,  // 115200
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        };

        uart_param_config(ECHO_UART_PORT_NUM, &uart_config);
        uart_set_pin(ECHO_UART_PORT_NUM, UART_ECHO_TXD, UART_ECHO_RXD,
                     UART_ECHO_RTS, UART_ECHO_CTS);
        uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE, BUF_SIZE, 0, NULL, 0);
    }

    // 机器人控制工具集成
    void InitializeTools() {
        // MCP工具注册，支持机器人移动控制
        mcp_server.RegisterTool("move_forward", [this](const cJSON* args) -> bool {
            // 发送前进命令到底盘
            SendRobotCommand("move_forward", args);
            return true;
        });

        mcp_server.RegisterTool("move_backward", [this](const cJSON* args) -> bool {
            SendRobotCommand("move_backward", args);
            return true;
        });

        // 摄像头拍照工具
        mcp_server.RegisterTool("take_photo", [this](const cJSON* args) -> bool {
            auto camera = GetCamera();
            if (camera) {
                std::vector<uint8_t> frame_data;
                return camera->CaptureFrame(frame_data);
            }
            return false;
        });
    }
};
```

### 2. ESP-Box (智能音箱开发板)

#### 高级音频配置
```cpp
// 高采样率音频 - 24kHz，适合高品质音频
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// 启用输入参考 - 用于回声消除
#define AUDIO_INPUT_REFERENCE    true

// 双音频编解码器支持
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR  // 播放
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR  // 录音

// 大尺寸显示屏 - 320x240
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true    // 需要镜像显示
#define DISPLAY_MIRROR_Y true
```

#### Box专用功能
```cpp
class EspBox3Board : public WifiBoard {
private:
    void InitializeBoxAudio() {
        // Box专用的BoxAudioCodec
        // 支持更高级的音频处理功能
        static BoxAudioCodec audio_codec(
            i2c_bus_, I2C_NUM_1,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN
        );
    }

    // 双击切换AEC模式（设备端回声消除）
    void InitializeButtons() {
        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
            }
        });
    }

    // ILI9341大屏LCD初始化
    void InitializeIli9341Display() {
        // 320x240高分辨率LCD配置
        // 支持触摸功能（部分版本）
    }
};
```

### 3. ESP-Hi (紧凑型开发板)

#### 小型化设计特点
```cpp
// PDM音频配置 - 适合小型化设计
#define AUDIO_ADC_MIC_CHANNEL       2           // ADC麦克风通道
#define AUDIO_PDM_SPEAK_P_GPIO      GPIO_NUM_6  // PDM扬声器+
#define AUDIO_PDM_SPEAK_N_GPIO      GPIO_NUM_7  // PDM扬声器-
#define AUDIO_PA_CTL_GPIO           GPIO_NUM_3  // 功放控制

// 多按钮设计
#define BOOT_BUTTON_GPIO            GPIO_NUM_9  // 启动按钮
#define MOVE_WAKE_BUTTON_GPIO       GPIO_NUM_0  // 移动唤醒按钮
#define AUDIO_WAKE_BUTTON_GPIO      GPIO_NUM_1  // 音频唤醒按钮

// 小尺寸OLED显示 - 160x80
#define DISPLAY_WIDTH           160
#define DISPLAY_HEIGHT          80
#define DISPLAY_MIRROR_Y        true
#define DISPLAY_SWAP_XY         true     // 坐标轴交换
#define DISPLAY_INVERT_COLOR    true     // 颜色反转

// 四个方向LED
#define FL_GPIO_NUM             GPIO_NUM_21  // 前左LED
#define FR_GPIO_NUM             GPIO_NUM_19  // 前右LED
#define BL_GPIO_NUM             GPIO_NUM_20  // 后左LED
#define BR_GPIO_NUM             GPIO_NUM_18  // 后右LED
```

#### Hi板特色实现
```cpp
class EspHiBoard : public WifiBoard {
private:
    // PDM音频实现
    void InitializePdmAudio() {
        // 使用PDM而非I2S，适合小型化
        // 配置ADC麦克风输入
        // 配置PDM扬声器输出
    }

    // 多按钮处理
    void InitializeMultipleButtons() {
        move_wake_button_.OnClick([this]() {
            // 移动唤醒功能
            TriggerMovementWakeup();
        });

        audio_wake_button_.OnClick([this]() {
            // 音频唤醒功能
            TriggerAudioWakeup();
        });
    }

    // 方向LED控制
    void InitializeDirectionalLeds() {
        // 4个方向LED用于状态指示
        // 可以显示方向、状态等信息
    }

    // 小尺寸显示优化
    void InitializeCompactDisplay() {
        // 160x80 ST7789 串行LCD
        // 优化UI布局适配小屏幕
    }
};
```

## 🏭 第三方厂商开发板分析

### M5Stack 生态系列

#### Atom S3 Echo Base
```cpp
// 超小型设计 - 24x24mm
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  128

// 圆形RGB LED矩阵
#define LED_MATRIX_SIZE     25      // 5x5 LED矩阵
#define LED_DATA_PIN        GPIO_NUM_35

// 音频配置
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000

class AtomS3EchoBase : public WifiBoard {
private:
    // LED矩阵控制
    void InitializeLedMatrix() {
        // 25个RGB LED的矩阵控制
        // 可显示图案、状态、动画等
    }

    // 紧凑型音频处理
    void InitializeCompactAudio() {
        // 针对小型化的音频优化
        // 功耗和性能平衡
    }
};
```

#### M5Stack Core S3
```cpp
// 标准M5Stack尺寸
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240

// 内置IMU和电池管理
#define IMU_I2C_ADDR    0x68
#define BATTERY_ADC_CHANNEL  7

class M5StackCoreS3 : public WifiBoard {
private:
    // IMU传感器集成
    void InitializeImu() {
        // 6轴IMU用于手势检测
        // 支持屏幕旋转等功能
    }

    // 电池电量监测
    void InitializeBatteryMonitor() {
        // 电池电压监测
        // 充电状态检测
    }

    // M5Stack生态兼容
    void InitializeM5Ecosystem() {
        // 与M5Stack外设兼容
        // Grove接口支持
    }
};
```

### WaveShare 系列

#### ESP32-S3 Touch AMOLED
```cpp
// 高质量AMOLED显示
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_TYPE_AMOLED

// 电容触摸支持
#define TOUCH_I2C_ADDR  0x15
#define TOUCH_INT_PIN   GPIO_NUM_21
#define TOUCH_RST_PIN   GPIO_NUM_11

class WaveShareS3TouchAmoled : public WifiBoard {
private:
    // AMOLED显示驱动
    void InitializeAmoledDisplay() {
        // 高对比度AMOLED
        // 触摸手势支持
        // 低功耗待机模式
    }

    // 高级触摸处理
    void InitializeTouchFeatures() {
        // 多点触摸支持
        // 手势识别
        // 触摸回调处理
    }
};
```

### 正点原子 (ATK) 系列

#### ATK-DNESP32S3-Box
```cpp
// 中文显示优化
#define DISPLAY_WIDTH   480
#define DISPLAY_HEIGHT  320
#define DISPLAY_CHINESE_FONT_SUPPORT

// 丰富的接口
#define CAN_TX_PIN      GPIO_NUM_21
#define CAN_RX_PIN      GPIO_NUM_22
#define RS485_TX_PIN    GPIO_NUM_17
#define RS485_RX_PIN    GPIO_NUM_18

class AtkDnEsp32s3Box : public WifiBoard {
private:
    // 中文显示优化
    void InitializeChineseDisplay() {
        // 中文字体支持
        // 输入法集成
        // 本地化UI
    }

    // 工业接口支持
    void InitializeIndustrialInterfaces() {
        // CAN总线支持
        // RS485通信
        // 工业级稳定性
    }
};
```

## 🔌 接口配置对比分析

### 音频接口对比
```cpp
// I2S标准配置 (大多数板卡)
ESP-SparkBot: ES8311, 16kHz, I2S
ESP-Box:      ES8311+ES7210, 24kHz, I2S + 参考输入
M5Stack:      内置编解码器, 16kHz, I2S

// PDM配置 (小型化板卡)
ESP-Hi:       PDM输出 + ADC输入, 16kHz
部分Atom:     PDM + 内置DAC

// 无音频配置 (显示专用板卡)
部分LCD评估板: NoAudioCodec
```

### 显示接口对比
```cpp
// SPI LCD (最常见)
240x240:      ESP-SparkBot, Atom系列
320x240:      ESP-Box, M5Stack
480x320:      ATK高端系列

// 串行小屏
160x80:       ESP-Hi
128x128:      部分紧凑型板卡

// AMOLED
240x240:      WaveShare AMOLED系列
触摸支持:      高端AMOLED板卡

// OLED
128x64:       部分简化版本
单色显示:      最小化设计
```

### 网络接口对比
```cpp
// 纯WiFi板卡 (60+个)
WifiBoard基类: 标准WiFi STA/AP模式

// 4G模块板卡 (5个)
ML307Board:   4G Cat-1网络
双卡双待:      部分高端版本

// 双网络板卡 (3个)
DualNetwork:  WiFi + 4G自动切换
智能选择:      根据信号质量切换
```

## ⚡ 性能优化对比

### 内存配置优化
```cpp
// PSRAM使用策略
高端板卡:    8MB PSRAM, CAMERA_FB_IN_PSRAM
中端板卡:    4MB PSRAM, 缓冲优化
低端板卡:    无PSRAM, CAMERA_FB_IN_DRAM
```

### 功耗管理优化
```cpp
// 电池供电板卡
SetPowerSaveMode实现:
- CPU频率调节
- 外设选择性关闭
- 睡眠模式管理

// 外部供电板卡
性能优先:
- 满频运行
- 全功能启用
- 实时响应优化
```

## 🎯 选择指南

### 按应用场景选择
```cpp
智能音箱应用:
└── ESP-Box系列 (高音质，大屏幕)

机器人应用:
└── ESP-SparkBot (摄像头，移动控制)

便携设备:
└── Atom系列 (小型化，低功耗)

工业应用:
└── ATK系列 (工业接口，可靠性)

开发调试:
└── 评估板系列 (丰富接口，易扩展)
```

### 按技术特性选择
```cpp
音频处理优先:
└── ESP-Box (24kHz，双编解码器)

显示效果优先:
└── WaveShare AMOLED (高对比度)

成本控制优先:
└── 面包板兼容版本 (标准化)

定制化需求:
└── 各厂商定制板卡
```

---

**相关文档**:
- [Board基类架构](./01-board-base.md)
- [音频编解码器系统](../03-audio-system/02-audio-codecs.md)
- [显示系统分析](../04-display-system/01-display-overview.md)
