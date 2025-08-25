# 唤醒词检测流程时序图

## 🎙️ Wake Word Detection Flow Sequence

这个时序图展示了xiaozhi-esp32从音频输入到唤醒词检测确认的完整流程，包括多级处理和状态变化。

```plantuml
@startuml WakeWord_Detection_Flow
title xiaozhi-esp32 唤醒词检测完整流程

participant "Microphone" as MIC
participant "AudioCodec" as CODEC
participant "AudioService" as SERVICE
participant "AudioProcessor" as PROCESSOR
participant "WakeWord" as WAKE
participant "Protocol" as PROTOCOL
participant "Application" as APP
participant "Display" as DISPLAY
participant "Server" as SERVER

== 音频输入和预处理 ==

loop 连续音频输入 (16kHz, 20ms帧)
    MIC -> CODEC: 模拟音频信号
    activate CODEC

    SERVICE -> CODEC: ReadData(input_data, 320)
    note right: 20ms@16kHz = 320样本
    CODEC -> SERVICE: PCM音频数据 [320 samples]

    SERVICE -> PROCESSOR: Feed(std::move(input_data))
    activate PROCESSOR

    PROCESSOR -> PROCESSOR: 降噪处理 (NS)
    PROCESSOR -> PROCESSOR: 回声消除 (AEC)
    PROCESSOR -> PROCESSOR: 语音活动检测 (VAD)

    alt VAD检测到语音
        PROCESSOR -> SERVICE: OnVadStateChange(true)
        SERVICE -> SERVICE: voice_detected_ = true
        note right: 进入语音检测状态
    end

    PROCESSOR -> SERVICE: 处理后的音频数据
    deactivate PROCESSOR

    SERVICE -> WAKE: Feed(processed_data)
    activate WAKE
end

== 唤醒词检测算法 ==

WAKE -> WAKE: 神经网络推理
note right: ESP WakeNet模型
note right: DET_MODE_95 (95%置信度)

alt 检测到疑似唤醒词
    WAKE -> WAKE: 置信度评估

    alt 置信度 > 阈值
        WAKE -> WAKE: 确认检测结果
        note right: wake_word_id > 0

        WAKE -> WAKE: 获取唤醒词字符串
        note right: GetWakeWordById(id)

        WAKE -> WAKE: 记录检测时间
        WAKE -> SERVICE: OnWakeWordDetected("xiaozhi")
        deactivate WAKE

        == 唤醒确认和状态变化 ==

        SERVICE -> SERVICE: 停止当前音频流
        SERVICE -> SERVICE: 设置唤醒状态

        SERVICE -> APP: OnWakeWordDetected("xiaozhi")
        activate APP

        APP -> APP: device_state_ = kDeviceStateListening
        APP -> APP: 记录唤醒时间戳

        APP -> DISPLAY: SetStatus("正在聆听...")
        activate DISPLAY
        DISPLAY -> DISPLAY: 更新UI状态
        DISPLAY -> DISPLAY: 显示聆听图标
        deactivate DISPLAY

        == 唤醒词音频编码 ==

        APP -> SERVICE: 请求唤醒词音频
        SERVICE -> WAKE: EncodeWakeWordData()
        activate WAKE

        WAKE -> WAKE: 提取唤醒词音频片段
        note right: 唤醒词前后各1-2秒

        WAKE -> WAKE: Opus编码音频数据
        note right: 24kbps, 20ms帧

        SERVICE -> WAKE: GetWakeWordOpus(opus_data)
        WAKE -> SERVICE: 返回编码后的音频
        deactivate WAKE

        == 服务器通信 ==

        SERVICE -> PROTOCOL: SendWakeWordDetected("xiaozhi")
        activate PROTOCOL

        PROTOCOL -> PROTOCOL: 构建JSON消息
        note right: {"type": "wake_word", "word": "xiaozhi"}

        alt WebSocket协议
            PROTOCOL -> SERVER: WebSocket文本消息
        else MQTT协议
            PROTOCOL -> SERVER: MQTT发布消息
        end

        alt 发送唤醒词音频
            SERVICE -> PROTOCOL: SendWakeWordAudio(opus_data)
            PROTOCOL -> PROTOCOL: 构建二进制音频包

            alt WebSocket协议
                PROTOCOL -> SERVER: WebSocket二进制消息
            else MQTT协议
                PROTOCOL -> SERVER: UDP加密音频流
            end
        end

        deactivate PROTOCOL

        == 服务器响应处理 ==

        SERVER -> PROTOCOL: 确认消息
        activate PROTOCOL
        PROTOCOL -> APP: OnServerResponse(response)

        alt 服务器确认唤醒
            APP -> APP: 准备音频通道
            APP -> SERVICE: StartAudioStreaming()

            APP -> DISPLAY: SetStatus("已连接，请说话...")

        else 服务器拒绝唤醒
            APP -> APP: device_state_ = kDeviceStateIdle
            APP -> DISPLAY: SetStatus("待机")

        end

        deactivate PROTOCOL
        deactivate APP

    else 置信度不足
        WAKE -> WAKE: 忽略检测结果
        note right: 继续监听
    end

else 未检测到唤醒词
    WAKE -> WAKE: 继续处理音频
    note right: 滑动窗口检测
end

== 异常情况处理 ==

alt 网络连接异常
    PROTOCOL -> APP: OnNetworkError("连接断开")
    APP -> DISPLAY: ShowNotification("网络错误", 3000)
    APP -> APP: 重置到待机状态
end

alt 音频处理异常
    PROCESSOR -> SERVICE: 处理失败
    SERVICE -> APP: OnAudioError("音频处理错误")
    APP -> SERVICE: 重启音频服务
end

alt 唤醒词模型异常
    WAKE -> SERVICE: 模型加载失败
    SERVICE -> APP: OnWakeWordError("模型错误")
    APP -> DISPLAY: ShowNotification("唤醒功能异常", 5000)
end

@enduml
```

## 🔍 关键技术点解析

### 1. 实时性能保证
```cpp
// 音频处理管道延迟控制
音频输入延迟:    < 5ms   (硬件缓冲)
信号处理延迟:    < 20ms  (AFE处理)
唤醒词检测延迟:  < 100ms (神经网络推理)
总体响应延迟:    < 150ms (从声音到状态变化)
```

### 2. 多级检测机制
```cpp
// 检测流程的多重验证
1. VAD预筛选:     确保有语音信号
2. 神经网络检测:  识别唤醒词模式
3. 置信度阈值:    DET_MODE_95 (95%置信度)
4. 时序验证:      避免误触发
5. 服务器确认:    最终验证 (可选)
```

### 3. 状态同步机制
```cpp
// 确保各模块状态一致
AudioService:    voice_detected_ = true
Application:     device_state_ = kDeviceStateListening
Display:         显示"正在聆听"状态
Protocol:        准备音频通道
```

### 4. 错误恢复策略
```cpp
// 各级异常处理
模型异常:        重新加载唤醒词模型
网络异常:        重连服务器，保持本地功能
音频异常:        重启音频服务
超时异常:        回到待机状态
```

## 🎯 性能优化要点

### 1. 内存管理
```cpp
// 音频数据的高效处理
std::move语义:     避免不必要的数据拷贝
预分配缓冲区:      减少动态内存分配
循环缓冲区:        平滑音频数据流
智能指针:          自动内存管理
```

### 2. CPU优化
```cpp
// 多核处理分工
Core 0: 唤醒词检测 + 应用逻辑
Core 1: 音频输入处理
AFE任务: 专用任务处理音频信号
LVGL任务: 专用任务处理UI更新
```

### 3. 功耗控制
```cpp
// 动态功耗管理
待机模式:          降低CPU频率
检测模式:          保持处理性能
省电模式:          选择性关闭功能
智能唤醒:          基于环境调整灵敏度
```

## 🔧 配置和调试

### 1. 唤醒词检测参数
```cpp
// 可调节的检测参数
detection_mode:    DET_MODE_90/95/99
threshold:         置信度阈值 (0.0-1.0)
window_size:       检测窗口大小
timeout:           检测超时时间
```

### 2. 音频处理参数
```cpp
// AFE处理器配置
aec_init:          回声消除开关
se_init:           语音增强开关
vad_init:          语音活动检测开关
vad_mode:          VAD敏感度模式
```

### 3. 调试工具
```cpp
// 调试和监控
音频数据转储:      保存检测音频用于分析
置信度日志:        记录检测置信度变化
状态跟踪:          监控各模块状态转换
性能统计:          测量各阶段处理时间
```

## 🎪 使用场景适配

### 1. 智能音箱场景
```cpp
// 高精度检测配置
detection_mode = DET_MODE_99;     // 最高精度
enable_aec = true;                // 启用回声消除
enable_se = true;                 // 启用语音增强
```

### 2. 便携设备场景
```cpp
// 平衡功耗和性能
detection_mode = DET_MODE_95;     // 平衡模式
low_power_mode = true;            // 省电模式
adaptive_threshold = true;        // 自适应阈值
```

### 3. 嘈杂环境场景
```cpp
// 增强抗噪能力
vad_mode = VAD_MODE_3;           // 最高灵敏度
noise_suppression_level = 3;      // 强降噪
detection_window_extend = true;    // 扩展检测窗口
```

---

**相关文档**:
- [唤醒词检测系统详解](../03-audio-system/04-wake-words.md)
- [音频处理器分析](../03-audio-system/03-audio-processors.md)
- [应用状态管理](../02-main-core/02-application-class.md)
- [音频处理流程图](./02-audio-processing-flow.md)
