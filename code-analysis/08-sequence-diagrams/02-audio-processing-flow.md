# 音频处理流程时序图

## 🎵 Audio Processing Flow Sequence

这个时序图展示了xiaozhi-esp32音频系统的完整处理流程，包括输入采集、处理、编码发送和接收解码播放。

```plantuml
@startuml Audio_Processing_Flow
title xiaozhi-esp32 音频处理完整流程

participant "Microphone" as MIC
participant "AudioCodec" as CODEC
participant "AudioService" as SERVICE
participant "AudioProcessor" as PROCESSOR
participant "WakeWord" as WAKE
participant "OpusEncoder" as ENCODER
participant "Protocol" as PROTOCOL
participant "Server" as SERVER
participant "OpusDecoder" as DECODER
participant "Speaker" as SPK

== 音频输入处理流程 ==

loop 音频输入任务 (Core 1, 优先级8)
    MIC -> CODEC: 模拟音频信号
    activate CODEC

    SERVICE -> CODEC: ReadData(input_data, FRAME_SIZE)
    CODEC -> SERVICE: PCM数据 (原始采样率)

    alt 需要重采样
        SERVICE -> SERVICE: input_resampler_.Process(input_data)
        note right: 重采样到16kHz
    end

    SERVICE -> PROCESSOR: Process(input_data)
    activate PROCESSOR
    PROCESSOR -> PROCESSOR: 降噪处理
    PROCESSOR -> PROCESSOR: 回声消除 (AEC)
    PROCESSOR -> PROCESSOR: 语音活动检测 (VAD)

    alt VAD状态变化
        PROCESSOR -> SERVICE: OnVadStateChange(speaking)
        SERVICE -> SERVICE: voice_detected_ = speaking
        SERVICE -> SERVICE: callbacks_.on_vad_change(speaking)
    end

    PROCESSOR -> SERVICE: 处理后的PCM数据
    deactivate PROCESSOR

    par 并行处理
        SERVICE -> WAKE: Process(input_data)
        activate WAKE
        WAKE -> WAKE: 唤醒词检测算法

        alt 检测到唤醒词
            WAKE -> SERVICE: OnWakeWordDetected(wake_word)
            SERVICE -> SERVICE: callbacks_.on_wake_word_detected(wake_word)
        end
        deactivate WAKE
    and
        alt 语音处理已启用
            SERVICE -> SERVICE: AddToEncodeQueue(input_data)
            note right: 添加到编码队列
        end
    end
    deactivate CODEC
end

== Opus编码和发送流程 ==

loop Opus编解码任务
    SERVICE -> SERVICE: ProcessEncodeQueue()

    alt 编码队列非空
        SERVICE -> ENCODER: Encode(pcm_data)
        activate ENCODER
        ENCODER -> ENCODER: Opus编码算法
        ENCODER -> SERVICE: AudioStreamPacket
        deactivate ENCODER

        SERVICE -> SERVICE: PushToSendQueue(packet)

        SERVICE -> PROTOCOL: SendAudioPacket(packet)
        activate PROTOCOL
        PROTOCOL -> SERVER: 发送Opus音频包
        deactivate PROTOCOL
    end
end

== 音频接收和解码流程 ==

SERVER -> PROTOCOL: 接收Opus音频包
activate PROTOCOL
PROTOCOL -> SERVICE: PushPacketToDecodeQueue(packet)
deactivate PROTOCOL

loop Opus解码处理
    SERVICE -> SERVICE: ProcessDecodeQueue()

    alt 解码队列非空
        SERVICE -> DECODER: Decode(packet)
        activate DECODER
        DECODER -> DECODER: Opus解码算法
        DECODER -> SERVICE: PCM数据
        deactivate DECODER

        SERVICE -> SERVICE: PushToPlaybackQueue(pcm_data)
    end
end

== 音频输出播放流程 ==

loop 音频输出任务 (Core 0, 优先级3)
    SERVICE -> SERVICE: PopFromPlaybackQueue()

    alt 播放队列非空
        alt 需要重采样
            SERVICE -> SERVICE: output_resampler_.Process(playback_data)
            note right: 重采样到输出采样率
        end

        SERVICE -> CODEC: WriteData(playback_data)
        activate CODEC
        CODEC -> SPK: 播放音频
        deactivate CODEC
    end
end

== 音频文件播放流程 ==

SERVICE -> SERVICE: PlaySound(ogg_data)
loop OGG文件解析
    SERVICE -> SERVICE: find_page(offset)
    SERVICE -> SERVICE: 解析OGG页面头

    alt Opus音频页面
        SERVICE -> DECODER: Decode(opus_packet)
        activate DECODER
        DECODER -> SERVICE: PCM数据
        deactivate DECODER
        SERVICE -> SERVICE: PushToPlaybackQueue(pcm_data)
    end
end

@enduml
```

## 🔍 流程详细说明

### 1. 音频输入处理 (实时流)

#### 核心步骤:
1. **硬件采集**: 麦克风 → ADC → AudioCodec
2. **数据读取**: AudioService从AudioCodec读取PCM数据
3. **重采样**: 将硬件采样率转换为16kHz标准采样率
4. **信号处理**: 降噪、回声消除、语音活动检测
5. **并行检测**: 唤醒词检测与语音编码同时进行

#### 性能特性:
- **任务优先级**: Core 1, 优先级8 (高优先级确保实时性)
- **帧大小**: 可配置的音频帧大小
- **延迟控制**: 最小化处理延迟

### 2. Opus编码和网络发送

#### 编码流程:
1. **队列管理**: 从编码队列获取PCM数据
2. **Opus编码**: 压缩PCM到Opus格式
3. **包装发送**: 添加时间戳等元数据
4. **网络传输**: 通过MQTT/WebSocket发送

#### 配置参数:
```cpp
#define OPUS_FRAME_DURATION_MS 60  // 60ms帧长
#define MAX_ENCODE_TASKS_IN_QUEUE 2  // 编码队列最大长度
```

### 3. 音频接收和解码

#### 解码流程:
1. **网络接收**: 从服务器接收Opus音频包
2. **队列缓冲**: 添加到解码队列进行缓冲
3. **Opus解码**: 解压缩为PCM数据
4. **播放准备**: 添加到播放队列

#### 缓冲策略:
- **解码队列**: 最大40个包 (2.4秒缓冲)
- **播放队列**: 最大2个任务
- **自适应缓冲**: 根据网络状况调整

### 4. 音频输出播放

#### 播放流程:
1. **队列管理**: 从播放队列获取PCM数据
2. **重采样**: 转换到硬件输出采样率
3. **硬件输出**: 写入AudioCodec → DAC → 扬声器

#### 输出特性:
- **任务优先级**: Core 0, 优先级3 (中等优先级)
- **连续播放**: 无缝音频播放
- **音量控制**: 支持动态音量调节

## ⚡ 性能优化点

### 多核架构优势
```
Core 1 (高性能核心):
├─ 音频输入任务 (优先级8)
├─ 音频处理算法
└─ 唤醒词检测

Core 0 (应用核心):
├─ 音频输出任务 (优先级3)
├─ Opus编解码任务
└─ 网络通信任务
```

### 内存管理策略
- **栈内存优先**: 减少动态内存分配
- **智能指针**: 自动管理音频包生命周期
- **队列限制**: 防止内存泄漏和溢出
- **零拷贝**: 尽可能减少数据拷贝

### 延迟优化技术
- **管道并行**: 采集、处理、编码并行进行
- **预分配缓冲**: 避免运行时内存分配
- **优化算法**: 使用ESP-AFE等优化算法库
- **硬件加速**: 利用ESP32的DSP指令

## 🎛️ 可配置参数

### 音频质量参数
```cpp
#define AUDIO_INPUT_SAMPLE_RATE  16000    // 输入采样率
#define AUDIO_OUTPUT_SAMPLE_RATE 16000    // 输出采样率
#define OPUS_FRAME_DURATION_MS   60       // Opus帧长
#define OPUS_COMPLEXITY          0        // 编码复杂度(0-10)
```

### 缓冲区配置
```cpp
#define MAX_ENCODE_TASKS_IN_QUEUE    2
#define MAX_PLAYBACK_TASKS_IN_QUEUE  2
#define MAX_DECODE_PACKETS_IN_QUEUE  40
#define MAX_SEND_PACKETS_IN_QUEUE    40
```

---

**相关文档**:
- [AudioService详细分析](../03-audio-system/01-audio-service.md)
- [音频编解码器详解](../03-audio-system/02-audio-codecs.md)
- [唤醒词检测流程](./03-wake-word-detection.md)
