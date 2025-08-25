# 音频处理器 (AudioProcessor) 系统详解

## 📁 模块概览
- **文件数量**: 6个文件 (3个.h + 3个.cc)
- **功能**: 音频信号处理，包括降噪、回声消除、语音活动检测
- **设计模式**: 策略模式 + 观察者模式
- **核心实现**: AfeAudioProcessor (基于ESP-AFE)

## 🏗️ 架构设计

### 继承层次结构
```
AudioProcessor (抽象基类)
├── AfeAudioProcessor        # ESP-AFE音频前端处理器
├── NoAudioProcessor         # 无处理器实现
└── AudioDebugger           # 音频调试器（辅助工具）
```

## 🎯 AudioProcessor基类分析

### 核心接口定义
```cpp
class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;

    // 生命周期管理
    virtual void Initialize(AudioCodec* codec, int frame_duration_ms) = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() = 0;

    // 数据处理
    virtual void Feed(std::vector<int16_t>&& data) = 0;
    virtual size_t GetFeedSize() = 0;

    // 事件回调
    virtual void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) = 0;
    virtual void OnVadStateChange(std::function<void(bool speaking)> callback) = 0;

    // 功能控制
    virtual void EnableDeviceAec(bool enable) = 0;
};
```

### 设计模式应用

#### 1. 策略模式
- **目的**: 支持不同的音频处理算法
- **实现**: 通过基类接口切换不同处理器
- **优势**: 运行时可以切换处理策略

#### 2. 观察者模式
- **OnOutput**: 处理完成的音频数据回调
- **OnVadStateChange**: 语音活动状态变化回调
- **好处**: 解耦音频处理和上层应用逻辑

## 🔊 AfeAudioProcessor 详细分析

### 功能概述
AfeAudioProcessor是基于ESP-AFE（Audio Front-End）的音频处理器实现，提供：
- **降噪处理** (Noise Suppression)
- **回声消除** (Acoustic Echo Cancellation)
- **语音活动检测** (Voice Activity Detection)
- **自动增益控制** (Automatic Gain Control)

### 核心成员变量
```cpp
class AfeAudioProcessor : public AudioProcessor {
private:
    // FreeRTOS事件组，用于任务同步
    EventGroupHandle_t event_group_ = nullptr;

    // ESP-AFE接口和数据
    esp_afe_sr_iface_t* afe_iface_ = nullptr;    // AFE接口
    esp_afe_sr_data_t* afe_data_ = nullptr;      // AFE数据结构

    // 回调函数
    std::function<void(std::vector<int16_t>&& data)> output_callback_;
    std::function<void(bool speaking)> vad_state_change_callback_;

    // 音频参数
    AudioCodec* codec_ = nullptr;                // 音频编解码器
    int frame_samples_ = 0;                      // 帧样本数
    bool is_speaking_ = false;                   // 当前是否在说话
    std::vector<int16_t> output_buffer_;         // 输出缓冲区
};
```

### 初始化流程分析

#### 1. Initialize 方法实现
```cpp
void AfeAudioProcessor::Initialize(AudioCodec* codec, int frame_duration_ms) {
    codec_ = codec;

    // 计算帧样本数
    frame_samples_ = (codec->input_sample_rate() * frame_duration_ms) / 1000;

    // 创建事件组
    event_group_ = xEventGroupCreate();

    // 初始化ESP-AFE
    afe_config_t afe_config = {
        .aec_init = true,              // 启用AEC
        .se_init = true,               // 启用语音增强
        .vad_init = true,              // 启用VAD
        .wakenet_init = false,         // 不启用唤醒网络
        .voice_communication_init = false,
        .voice_communication_agc_init = false,
        .voice_communication_agc_gain = 15,
        .vad_mode = VAD_MODE_3,        // VAD模式3（高灵敏度）
        .wakenet_mode = DET_MODE_90,
        .afe_mode = SR_MODE_LOW_COST,  // 低成本模式
        .afe_perferred_core = 0,       // 使用核心0
        .afe_perferred_priority = 5,   // 任务优先级5
        .afe_ringbuf_size = 50,        // 环形缓冲区大小
        .memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM,
        .afe_linear_gain = 1.0,
        .afe_ser_timeout = portMAX_DELAY,
        .afe_task_stack = 4 * 1024,    // 任务栈大小4KB
    };

    // 获取AFE接口
    afe_iface_ = &ESP_AFE_SR_HANDLE;

    // 创建AFE数据结构
    afe_data_ = afe_iface_->create_from_config(&afe_config);

    // 预分配输出缓冲区
    output_buffer_.reserve(frame_samples_);
}
```

#### 2. 配置参数详解
```cpp
typedef struct {
    bool aec_init;                    // 是否启用回声消除
    bool se_init;                     // 是否启用语音增强
    bool vad_init;                    // 是否启用语音活动检测
    bool wakenet_init;                // 是否启用唤醒网络
    bool voice_communication_init;    // 是否启用语音通信
    bool voice_communication_agc_init; // 是否启用语音通信AGC
    int voice_communication_agc_gain; // 语音通信AGC增益
    vad_mode_t vad_mode;             // VAD模式
    det_mode_t wakenet_mode;         // 唤醒网络模式
    afe_mode_t afe_mode;             // AFE模式
    int afe_perferred_core;          // 首选CPU核心
    int afe_perferred_priority;      // 任务优先级
    int afe_ringbuf_size;            // 环形缓冲区大小
    afe_memory_alloc_mode_t memory_alloc_mode; // 内存分配模式
    float afe_linear_gain;           // 线性增益
    int afe_ser_timeout;             // 超时设置
    int afe_task_stack;              // 任务栈大小
} afe_config_t;
```

### 音频处理流程

#### 1. Feed 方法 - 数据输入
```cpp
void AfeAudioProcessor::Feed(std::vector<int16_t>&& data) {
    if (!afe_data_ || !IsRunning()) {
        return;
    }

    // 将数据送入AFE处理
    int afe_chunksize = afe_iface_->get_feed_chunksize(afe_data_);
    if (data.size() >= afe_chunksize) {
        afe_iface_->feed(afe_data_, data.data());

        // 触发处理任务
        xEventGroupSetBits(event_group_, BIT0);
    }
}
```

#### 2. AudioProcessorTask - 处理任务
```cpp
void AfeAudioProcessor::AudioProcessorTask() {
    while (IsRunning()) {
        // 等待数据到达
        EventBits_t bits = xEventGroupWaitBits(
            event_group_, BIT0,
            pdTRUE, pdFALSE,
            portMAX_DELAY
        );

        if (bits & BIT0) {
            // 获取处理后的音频数据
            int fetch_len = afe_iface_->get_fetch_chunksize(afe_data_);
            output_buffer_.resize(fetch_len);

            int ret = afe_iface_->fetch(afe_data_, output_buffer_.data());
            if (ret > 0) {
                output_buffer_.resize(ret);

                // 检查VAD状态变化
                bool current_speaking = afe_iface_->get_vad_state(afe_data_);
                if (current_speaking != is_speaking_) {
                    is_speaking_ = current_speaking;
                    if (vad_state_change_callback_) {
                        vad_state_change_callback_(is_speaking_);
                    }
                }

                // 输出处理后的数据
                if (output_callback_) {
                    output_callback_(std::move(output_buffer_));
                }
            }
        }
    }
}
```

### ESP-AFE 核心功能

#### 1. 回声消除 (AEC)
```cpp
// AEC算法特点
- **自适应滤波**: 根据环境自动调整
- **非线性处理**: 处理非线性回声
- **延迟补偿**: 自动补偿播放延迟
- **多通道支持**: 支持立体声回声消除
```

#### 2. 语音增强 (SE)
```cpp
// 语音增强特性
- **谱减法**: 减少背景噪声
- **维纳滤波**: 优化信噪比
- **语音检测**: 区分语音和噪声
- **动态范围压缩**: 平衡音量
```

#### 3. 语音活动检测 (VAD)
```cpp
// VAD模式配置
typedef enum {
    VAD_MODE_0 = 0,    // 最不敏感
    VAD_MODE_1,        // 低敏感度
    VAD_MODE_2,        // 中敏感度
    VAD_MODE_3,        // 高敏感度（默认）
} vad_mode_t;

// VAD状态查询
bool is_speaking = afe_iface_->get_vad_state(afe_data_);
```

## 🚫 NoAudioProcessor 实现

### 空处理器设计
```cpp
class NoAudioProcessor : public AudioProcessor {
public:
    void Initialize(AudioCodec* codec, int frame_duration_ms) override {
        codec_ = codec;
        frame_samples_ = (codec->input_sample_rate() * frame_duration_ms) / 1000;
    }

    void Feed(std::vector<int16_t>&& data) override {
        // 直接透传数据，不做任何处理
        if (output_callback_) {
            output_callback_(std::move(data));
        }
    }

    void Start() override { running_ = true; }
    void Stop() override { running_ = false; }
    bool IsRunning() override { return running_; }

    void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) override {
        output_callback_ = callback;
    }

    void OnVadStateChange(std::function<void(bool speaking)> callback) override {
        vad_callback_ = callback;
        // NoProcessor始终认为在说话
        if (callback) callback(true);
    }

    size_t GetFeedSize() override { return frame_samples_; }
    void EnableDeviceAec(bool enable) override { /* 无操作 */ }

private:
    AudioCodec* codec_ = nullptr;
    bool running_ = false;
    int frame_samples_ = 0;
    std::function<void(std::vector<int16_t>&& data)> output_callback_;
    std::function<void(bool speaking)> vad_callback_;
};
```

## 🔧 AudioDebugger 调试工具

### 调试功能
```cpp
class AudioDebugger {
public:
    // 音频数据记录
    void RecordInputData(const std::vector<int16_t>& data);
    void RecordOutputData(const std::vector<int16_t>& data);

    // 统计信息
    void GetStatistics(AudioStats& stats);

    // 数据导出
    void ExportWaveFile(const std::string& filename);
    void ExportRawData(const std::string& filename);

private:
    std::vector<int16_t> input_buffer_;
    std::vector<int16_t> output_buffer_;
    size_t total_samples_ = 0;
    std::chrono::steady_clock::time_point start_time_;
};
```

## ⚡ 性能优化

### 内存管理
```cpp
// PSRAM优先分配
.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM,

// 缓冲区预分配
output_buffer_.reserve(frame_samples_);

// 移动语义减少拷贝
void Feed(std::vector<int16_t>&& data);
output_callback_(std::move(output_buffer_));
```

### 实时性保证
```cpp
// 高优先级任务
.afe_perferred_priority = 5,

// 专用CPU核心
.afe_perferred_core = 0,

// 大的任务栈
.afe_task_stack = 4 * 1024,

// 事件驱动处理
xEventGroupSetBits(event_group_, BIT0);
```

### 算法优化
```cpp
// 低成本模式
.afe_mode = SR_MODE_LOW_COST,

// 环形缓冲区优化
.afe_ringbuf_size = 50,

// 自适应超时
.afe_ser_timeout = portMAX_DELAY,
```

## 🔗 与其他模块集成

### AudioService集成
```cpp
void AudioService::Initialize() {
    // 创建音频处理器
    if (processor_enabled_) {
        audio_processor_ = std::make_unique<AfeAudioProcessor>();
    } else {
        audio_processor_ = std::make_unique<NoAudioProcessor>();
    }

    // 初始化处理器
    audio_processor_->Initialize(audio_codec_, FRAME_DURATION_MS);

    // 设置回调
    audio_processor_->OnOutput([this](std::vector<int16_t>&& data) {
        OnProcessedAudio(std::move(data));
    });

    audio_processor_->OnVadStateChange([this](bool speaking) {
        OnVadStateChange(speaking);
    });
}
```

### 配置管理
```cpp
// 从配置文件读取处理器设置
Settings settings("audio", true);
bool enable_aec = settings.GetBool("enable_aec", true);
bool enable_vad = settings.GetBool("enable_vad", true);
int vad_mode = settings.GetInt("vad_mode", 3);

// 应用配置
audio_processor_->EnableDeviceAec(enable_aec);
```

## 🎯 使用场景分析

### 1. 智能音箱场景
```cpp
// 启用完整音频处理
AfeAudioProcessor processor;
processor.Initialize(codec, 60);  // 60ms帧
processor.EnableDeviceAec(true);  // 启用回声消除
// VAD模式3，高灵敏度
```

### 2. 语音通话场景
```cpp
// 优化语音通信
afe_config.voice_communication_init = true;
afe_config.voice_communication_agc_init = true;
afe_config.voice_communication_agc_gain = 15;
```

### 3. 低功耗场景
```cpp
// 使用简化处理
NoAudioProcessor processor;
// 或者禁用部分AFE功能
afe_config.se_init = false;  // 禁用语音增强
```

---

**相关文档**:
- [AudioService核心分析](./01-audio-service.md)
- [音频编解码器详解](./02-audio-codecs.md)
- [唤醒词检测系统](./04-wake-words.md)
