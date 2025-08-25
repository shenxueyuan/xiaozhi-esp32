# 音频编解码器 (AudioCodec) 系统详解

## 📁 模块概览
- **文件数量**: 14个文件 (7个.h + 7个.cc)
- **功能**: 硬件音频芯片的抽象和驱动实现
- **设计模式**: 策略模式 + 工厂模式
- **支持芯片**: ES8311, ES8374, ES8388, ES8389, Box系列等

## 🏗️ 架构设计

### 继承层次结构
```
AudioCodec (抽象基类)
├── Es8311AudioCodec      # ES8311芯片
├── Es8374AudioCodec      # ES8374芯片
├── Es8388AudioCodec      # ES8388芯片
├── Es8389AudioCodec      # ES8389芯片
├── BoxAudioCodec         # ESP-Box系列
├── NoAudioCodec          # 无音频功能
└── DummyAudioCodec       # 虚拟音频（测试用）
```

## 🎯 AudioCodec基类分析

### 核心接口定义
```cpp
class AudioCodec {
public:
    AudioCodec();
    virtual ~AudioCodec();

    // 控制接口
    virtual void SetOutputVolume(int volume);
    virtual void EnableInput(bool enable);
    virtual void EnableOutput(bool enable);
    virtual void Start();

    // 数据接口
    virtual void OutputData(std::vector<int16_t>& data);
    virtual bool InputData(std::vector<int16_t>& data);

    // 状态查询
    inline bool duplex() const { return duplex_; }
    inline bool input_reference() const { return input_reference_; }
    inline int input_sample_rate() const { return input_sample_rate_; }
    inline int output_sample_rate() const { return output_sample_rate_; }
    inline int input_channels() const { return input_channels_; }
    inline int output_channels() const { return output_channels_; }
    inline int output_volume() const { return output_volume_; }
    inline bool input_enabled() const { return input_enabled_; }
    inline bool output_enabled() const { return output_enabled_; }

protected:
    // I2S通道句柄
    i2s_chan_handle_t tx_handle_ = nullptr;
    i2s_chan_handle_t rx_handle_ = nullptr;

    // 音频参数
    bool duplex_ = false;              // 是否全双工
    bool input_reference_ = false;     // 是否输入参考
    bool input_enabled_ = false;       // 输入是否启用
    bool output_enabled_ = false;      // 输出是否启用
    int input_sample_rate_ = 0;        // 输入采样率
    int output_sample_rate_ = 0;       // 输出采样率
    int input_channels_ = 1;           // 输入通道数
    int output_channels_ = 1;          // 输出通道数
    int output_volume_ = 70;           // 输出音量

    // 纯虚函数，由子类实现
    virtual int Read(int16_t* dest, int samples) = 0;
    virtual int Write(const int16_t* data, int samples) = 0;
};
```

### 关键设计特性

#### 1. 统一接口抽象
- **数据接口**: 统一的PCM数据读写接口
- **控制接口**: 标准化的启用/禁用和音量控制
- **状态接口**: 一致的状态查询方法

#### 2. I2S硬件抽象
- **双通道支持**: tx_handle_用于播放，rx_handle_用于录音
- **全双工模式**: duplex_标志支持同时录音和播放
- **参数统一**: 采样率、通道数等参数标准化

#### 3. 常量定义
```cpp
#define AUDIO_CODEC_DMA_DESC_NUM 6        // DMA描述符数量
#define AUDIO_CODEC_DMA_FRAME_NUM 240     // DMA帧数量
#define AUDIO_CODEC_DEFAULT_MIC_GAIN 30.0 // 默认麦克风增益
```

## 🔊 ES8311AudioCodec 详细分析

### 文件信息
- **路径**: `main/audio/codecs/es8311_audio_codec.h/cc`
- **芯片**: ES8311 - 低功耗音频编解码器
- **特性**: 支持I2S接口，内置ADC/DAC，支持麦克风和扬声器

### 核心实现架构

#### 1. 私有成员结构
```cpp
class Es8311AudioCodec : public AudioCodec {
private:
    // ESP编解码器框架接口
    const audio_codec_data_if_t* data_if_ = nullptr;     // 数据接口
    const audio_codec_ctrl_if_t* ctrl_if_ = nullptr;     // 控制接口
    const audio_codec_if_t* codec_if_ = nullptr;         // 编解码器接口
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;     // GPIO接口

    // 设备句柄和配置
    esp_codec_dev_handle_t dev_ = nullptr;               // 编解码器设备句柄
    gpio_num_t pa_pin_ = GPIO_NUM_NC;                    // 功放控制引脚
    bool pa_inverted_ = false;                           // 功放信号是否反相
    std::mutex data_if_mutex_;                           // 数据接口互斥锁

    // 私有方法
    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk,
                             gpio_num_t ws, gpio_num_t dout, gpio_num_t din);
    void UpdateDeviceState();
};
```

#### 2. 构造函数参数详解
```cpp
Es8311AudioCodec(
    void* i2c_master_handle,     // I2C主设备句柄
    i2c_port_t i2c_port,        // I2C端口
    int input_sample_rate,       // 输入采样率
    int output_sample_rate,      // 输出采样率
    gpio_num_t mclk,            // 主时钟引脚
    gpio_num_t bclk,            // 位时钟引脚
    gpio_num_t ws,              // 字选择引脚
    gpio_num_t dout,            // 数据输出引脚
    gpio_num_t din,             // 数据输入引脚
    gpio_num_t pa_pin,          // 功放控制引脚
    uint8_t es8311_addr,        // ES8311 I2C地址
    bool use_mclk = true,       // 是否使用主时钟
    bool pa_inverted = false    // 功放信号是否反相
);
```

#### 3. 重写的虚函数
```cpp
virtual void SetOutputVolume(int volume) override;
virtual void EnableInput(bool enable) override;
virtual void EnableOutput(bool enable) override;
virtual int Read(int16_t* dest, int samples) override;
virtual int Write(const int16_t* data, int samples) override;
```

### ES8311初始化流程分析

#### 1. I2S接口创建
```cpp
void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk,
                         gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
    // 1. 配置I2S时钟
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = &rx_handle_,
        .tx_handle = &tx_handle_,
        .clk_cfg = {
            .sample_rate = output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = dout,
            .din = din,
        },
    };

    // 2. 创建数据接口
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
}
```

#### 2. ES8311芯片配置
```cpp
// ES8311专用配置结构
es8311_codec_cfg_t es8311_cfg = {
    .ctrl_if = ctrl_if_,                    // 控制接口
    .pa_pin = pa_pin,                       // 功放控制引脚
    .use_mclk = use_mclk,                   // 使用主时钟
    .hw_gain = {
        .pa_voltage = 5.0,                  // 功放电压
        .codec_dac_voltage = 3.3,           // DAC电压
    },
    .pa_reverted = pa_inverted_,            // 功放反相
};

// 创建编解码器接口
codec_if_ = es8311_codec_new(&es8311_cfg);
```

#### 3. 设备状态更新
```cpp
void UpdateDeviceState() {
    // 更新输入状态
    if (input_enabled_) {
        esp_codec_dev_set_in_gain(dev_, AUDIO_CODEC_DEFAULT_MIC_GAIN);
    }

    // 更新输出状态
    if (output_enabled_) {
        esp_codec_dev_set_out_vol(dev_, output_volume_);
    }
}
```

## 🎵 其他编解码器实现

### ES8374AudioCodec
- **特点**: 高性能音频编解码器
- **应用**: ESP32-S3-Korvo-2等高端开发板
- **优势**: 更好的音质和更多的音频处理功能
- **文件**: `es8374_audio_codec.h/cc`

### ES8388AudioCodec
- **特点**: 立体声音频编解码器
- **应用**: ESP-Box系列开发板
- **优势**: 支持立体声，音质优秀
- **文件**: `es8388_audio_codec.h/cc`

### ES8389AudioCodec
- **特点**: 增强型音频编解码器
- **应用**: 高端音频应用
- **优势**: 更多音频特性和更高精度
- **文件**: `es8389_audio_codec.h/cc`

### BoxAudioCodec
- **特点**: ESP-Box专用音频解决方案
- **集成**: 与ESP-Box硬件深度集成
- **优化**: 针对ESP-Box的特殊优化
- **文件**: `box_audio_codec.h/cc`

### NoAudioCodec
```cpp
class NoAudioCodec : public AudioCodec {
public:
    void SetOutputVolume(int volume) override {}
    void EnableInput(bool enable) override {}
    void EnableOutput(bool enable) override {}
    void OutputData(std::vector<int16_t>& data) override {}
    bool InputData(std::vector<int16_t>& data) override { return false; }
    void Start() override {}

protected:
    int Read(int16_t* dest, int samples) override { return 0; }
    int Write(const int16_t* data, int samples) override { return 0; }
};
```

### DummyAudioCodec
```cpp
class DummyAudioCodec : public AudioCodec {
    // 提供虚拟音频数据，用于测试和调试
    bool InputData(std::vector<int16_t>& data) override {
        data.resize(240);
        // 生成测试音频数据（如正弦波）
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<int16_t>(
                32767 * sin(2 * M_PI * 1000 * i / input_sample_rate_)
            );
        }
        return true;
    }
};
```

## 🔧 硬件接口适配

### I2S配置详解
```cpp
// I2S时钟配置
.clk_cfg = {
    .sample_rate = output_sample_rate_,      // 采样率
    .clk_src = I2S_CLK_SRC_DEFAULT,          // 时钟源
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,  // 主时钟倍数
}

// I2S槽位配置
.slot_cfg = {
    .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,  // 数据位宽
    .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,   // 槽位位宽
    .slot_mode = I2S_SLOT_MODE_STEREO,           // 立体声模式
    .slot_mask = I2S_STD_SLOT_BOTH,              // 左右声道
}

// GPIO引脚配置
.gpio_cfg = {
    .mclk = mclk,    // 主时钟 (可选)
    .bclk = bclk,    // 位时钟
    .ws = ws,        // 字选择/帧同步
    .dout = dout,    // 数据输出 (播放)
    .din = din,      // 数据输入 (录音)
}
```

### I2C控制接口
```cpp
// ES8311寄存器控制
typedef struct {
    audio_codec_ctrl_if_t *ctrl_if;    // 控制接口
    gpio_num_t pa_pin;                 // 功放控制引脚
    bool use_mclk;                     // 是否使用主时钟
    es8311_codec_hw_gain_t hw_gain;    // 硬件增益配置
    bool pa_reverted;                  // 功放控制反相
} es8311_codec_cfg_t;
```

## ⚡ 性能优化特性

### 缓冲管理
- **DMA缓冲**: 使用DMA减少CPU负载
- **双缓冲**: 防止音频断续
- **队列管理**: 平滑的数据流控制

### 功耗优化
- **动态启用**: 按需启用输入/输出
- **功放控制**: 智能功放开关控制
- **时钟管理**: 优化时钟配置减少功耗

### 延迟优化
- **最小缓冲**: 减少音频延迟
- **中断驱动**: 实时响应音频事件
- **优先级调度**: 音频任务高优先级

## 🔍 故障诊断和调试

### 常见问题

#### 1. I2C通信失败
```cpp
// 检查I2C连接和地址
E (786) i2c.master: I2C transaction unexpected nack detected
E (796) I2C_If: Fail to write to dev 30
```
**解决方案**: 检查I2C引脚连接，确认设备地址

#### 2. I2S数据流错误
```cpp
// 检查I2S配置和GPIO
E (1234) I2S: i2s_channel_read timeout
```
**解决方案**: 检查I2S引脚配置，确认时钟同步

#### 3. 音频质量问题
- **采样率不匹配**: 确保输入输出采样率正确
- **时钟抖动**: 检查MCLK稳定性
- **信号完整性**: 检查硬件信号质量

### 调试工具
```cpp
// 启用音频调试日志
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

// 音频数据转储
void AudioCodec::DumpAudioData(const std::vector<int16_t>& data) {
    ESP_LOGD(TAG, "Audio data: samples=%zu, first=%d, last=%d",
             data.size(), data[0], data[data.size()-1]);
}
```

## 🔗 与其他模块的集成

### AudioService集成
```cpp
// AudioService中的编解码器使用
void AudioService::Initialize(AudioCodec* codec) {
    codec_ = codec;
    codec_->Start();

    // 设置音频参数
    if (codec->input_sample_rate() != 16000) {
        input_resampler_.Configure(codec->input_sample_rate(), 16000);
    }
}
```

### Board集成
```cpp
// 在Board实现中创建特定的编解码器
virtual AudioCodec* GetAudioCodec() override {
    static Es8311AudioCodec audio_codec(
        i2c_bus_, I2C_NUM_0,
        AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
        AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
        AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
        AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR
    );
    return &audio_codec;
}
```

---

**相关文档**:
- [AudioService核心分析](./01-audio-service.md)
- [音频处理器详解](./03-audio-processors.md)
- [硬件抽象层分析](../05-board-abstraction/01-board-base.md)
