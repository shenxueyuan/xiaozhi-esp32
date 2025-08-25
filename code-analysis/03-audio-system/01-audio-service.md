# AudioService 类 - 音频服务核心

## 📁 文件信息
- **路径**: `main/audio/audio_service.h` + `main/audio/audio_service.cc`
- **类型**: 音频处理服务
- **功能**: 音频系统的核心控制器，管理音频采集、处理、编解码、播放

## 🎯 核心功能

### 类定义概览
```cpp
class AudioService {
public:
    void Initialize(AudioCodec* codec);
    void Start();
    void Stop();

    // 音频控制
    void EnableWakeWordDetection(bool enable);
    void EnableVoiceProcessing(bool enable);
    void EnableAudioTesting(bool enable);
    void EnableDeviceAec(bool enable);

    // 音频数据处理
    bool PushPacketToDecodeQueue(std::unique_ptr<AudioStreamPacket> packet);
    std::unique_ptr<AudioStreamPacket> PopPacketFromSendQueue();
    void PlaySound(const std::string_view& sound);

    // 状态查询
    bool IsVoiceDetected() const;
    bool IsIdle();
    bool IsWakeWordRunning() const;

private:
    AudioCodec* codec_;
    std::unique_ptr<AudioProcessor> audio_processor_;
    std::unique_ptr<WakeWord> wake_word_;
    std::unique_ptr<OpusEncoderWrapper> opus_encoder_;
    std::unique_ptr<OpusDecoderWrapper> opus_decoder_;
    // ... 更多成员
};
```

## 🏗️ 音频架构设计

### 双向音频流水线
```
输入流水线：
麦克风 → AudioCodec → AudioProcessor → OpusEncoder → 发送队列 → 网络

输出流水线：
网络 → 解码队列 → OpusDecoder → 播放队列 → AudioCodec → 扬声器
```

### 任务架构
```cpp
// 音频输入任务 (Core 1, 优先级8)
void AudioInputTask();

// 音频输出任务 (Core 0, 优先级3)
void AudioOutputTask();

// Opus编解码任务
void OpusEncodeDecodeTask();
```

## 🔗 关键依赖

### 主要组件
```cpp
#include "audio_codec.h"        // 音频编解码器抽象
#include "audio_processor.h"    // 音频处理器抽象
#include "wake_word.h"          // 唤醒词检测
#include <opus_encoder.h>       // Opus编码器
#include <opus_decoder.h>       // Opus解码器
#include <opus_resampler.h>     // Opus重采样器
```

### 依赖关系图
```
AudioService
    ├─→ AudioCodec (硬件抽象)
    ├─→ AudioProcessor (信号处理)
    ├─→ WakeWord (唤醒词检测)
    ├─→ OpusEncoder (音频编码)
    ├─→ OpusDecoder (音频解码)
    └─→ OpusResampler (重采样)
```

## 📋 初始化流程

### Initialize() 方法详解
```cpp
void AudioService::Initialize(AudioCodec* codec) {
    codec_ = codec;
    codec_->Start();

    // 创建Opus编解码器
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(
        codec->output_sample_rate(), 1, OPUS_FRAME_DURATION_MS);
    opus_encoder_ = std::make_unique<OpusEncoderWrapper>(
        16000, 1, OPUS_FRAME_DURATION_MS);
    opus_encoder_->SetComplexity(0);

    // 配置重采样器
    if (codec->input_sample_rate() != 16000) {
        input_resampler_.Configure(codec->input_sample_rate(), 16000);
        reference_resampler_.Configure(codec->input_sample_rate(), 16000);
    }

    // 创建音频处理器
#if CONFIG_USE_AUDIO_PROCESSOR
    audio_processor_ = std::make_unique<AfeAudioProcessor>();
#else
    audio_processor_ = std::make_unique<NoAudioProcessor>();
#endif

    // 创建唤醒词检测器
#if CONFIG_USE_AFE_WAKE_WORD
    wake_word_ = std::make_unique<AfeWakeWord>();
#elif CONFIG_USE_ESP_WAKE_WORD
    wake_word_ = std::make_unique<EspWakeWord>();
#elif CONFIG_USE_CUSTOM_WAKE_WORD
    wake_word_ = std::make_unique<CustomWakeWord>();
#endif

    // 设置回调函数
    SetupCallbacks();
}
```

## 🎵 音频处理流水线

### 输入处理流程
```cpp
void AudioService::AudioInputTask() {
    while (!service_stopped_) {
        // 1. 从音频编解码器读取数据
        std::vector<int16_t> input_data;
        codec_->ReadData(input_data, FRAME_SIZE);

        // 2. 重采样到16kHz (如果需要)
        if (input_resampler_.IsConfigured()) {
            input_resampler_.Process(input_data);
        }

        // 3. 音频处理 (降噪、AEC等)
        audio_processor_->Process(input_data);

        // 4. 唤醒词检测
        if (wake_word_) {
            wake_word_->Process(input_data);
        }

        // 5. 添加到编码队列
        if (voice_processing_enabled_) {
            AddToEncodeQueue(std::move(input_data));
        }
    }
}
```

### 输出播放流程
```cpp
void AudioService::AudioOutputTask() {
    while (!service_stopped_) {
        // 1. 从播放队列获取数据
        auto playback_data = PopFromPlaybackQueue();
        if (!playback_data) continue;

        // 2. 重采样到输出采样率 (如果需要)
        if (output_resampler_.IsConfigured()) {
            output_resampler_.Process(*playback_data);
        }

        // 3. 写入音频编解码器
        codec_->WriteData(*playback_data);
    }
}
```

## 🔊 Opus编解码处理

### 编码流程
```cpp
void AudioService::OpusEncodeDecodeTask() {
    while (!service_stopped_) {
        // 处理编码任务
        ProcessEncodeQueue();

        // 处理解码任务
        ProcessDecodeQueue();

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void AudioService::ProcessEncodeQueue() {
    auto encode_task = PopFromEncodeQueue();
    if (!encode_task) return;

    // Opus编码
    auto packet = opus_encoder_->Encode(encode_task->pcm);
    if (packet) {
        packet->timestamp = encode_task->timestamp;
        PushToSendQueue(std::move(packet));
    }
}
```

### 解码流程
```cpp
void AudioService::ProcessDecodeQueue() {
    auto packet = PopFromDecodeQueue();
    if (!packet) return;

    // Opus解码
    auto pcm_data = opus_decoder_->Decode(*packet);
    if (pcm_data) {
        PushToPlaybackQueue(std::move(pcm_data));
    }
}
```

## 👂 唤醒词检测集成

### 唤醒词回调设置
```cpp
if (wake_word_) {
    wake_word_->OnWakeWordDetected([this](const std::string& wake_word) {
        if (callbacks_.on_wake_word_detected) {
            callbacks_.on_wake_word_detected(wake_word);
        }
    });
}
```

### 支持的唤醒词算法
1. **AfeWakeWord** - ESP-AFE算法库
2. **EspWakeWord** - ESP官方算法
3. **CustomWakeWord** - 自定义算法

## 🎛️ 语音活动检测 (VAD)

### VAD回调机制
```cpp
audio_processor_->OnVadStateChange([this](bool speaking) {
    voice_detected_ = speaking;
    if (callbacks_.on_vad_change) {
        callbacks_.on_vad_change(speaking);
    }
});
```

### VAD状态管理
- **检测到语音**: 开始录音和处理
- **语音结束**: 停止录音，发送完整语音包
- **状态通知**: 通知应用层语音状态变化

## 🔧 队列管理

### 音频数据队列
```cpp
// 编码队列：PCM → Opus
std::deque<AudioTask> encode_queue_;

// 发送队列：Opus包 → 网络
std::deque<std::unique_ptr<AudioStreamPacket>> send_queue_;

// 解码队列：网络 → Opus包
std::deque<std::unique_ptr<AudioStreamPacket>> decode_queue_;

// 播放队列：Opus → PCM
std::deque<std::vector<int16_t>> playback_queue_;
```

### 队列大小限制
```cpp
#define MAX_ENCODE_TASKS_IN_QUEUE 2
#define MAX_PLAYBACK_TASKS_IN_QUEUE 2
#define MAX_DECODE_PACKETS_IN_QUEUE (2400 / OPUS_FRAME_DURATION_MS)
#define MAX_SEND_PACKETS_IN_QUEUE (2400 / OPUS_FRAME_DURATION_MS)
```

## 🎼 音频播放功能

### PlaySound() 实现
```cpp
void AudioService::PlaySound(const std::string_view& ogg) {
    // 1. 启用输出
    if (!codec_->output_enabled()) {
        codec_->EnableOutput(true);
    }

    // 2. 解析OGG文件
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(ogg.data());

    // 3. 查找OGG页面
    auto find_page = [&](size_t start) -> size_t {
        for (size_t i = start; i + 4 <= size; ++i) {
            if (buf[i] == 'O' && buf[i+1] == 'g' &&
                buf[i+2] == 'g' && buf[i+3] == 'S') return i;
        }
        return static_cast<size_t>(-1);
    };

    // 4. 解码并播放
    while (/* 处理所有OGG页面 */) {
        // 解析Opus包并添加到解码队列
    }
}
```

## ⚡ 性能优化

### 多核利用
- **Core 1**: 音频输入任务 (高优先级)
- **Core 0**: 音频输出任务 (中优先级)
- **Opus处理**: 独立任务，平衡负载

### 内存优化
- 使用栈内存缓冲区减少动态分配
- 智能指针自动管理音频包生命周期
- 队列大小限制防止内存溢出

### 延迟优化
- 60ms Opus帧长平衡质量和延迟
- 优化的重采样算法
- 最小化锁等待时间

---

**相关文档**:
- [音频编解码器分析](./02-audio-codecs.md)
- [音频处理器分析](./03-audio-processors.md)
- [唤醒词检测分析](./04-wake-word-detection.md)
