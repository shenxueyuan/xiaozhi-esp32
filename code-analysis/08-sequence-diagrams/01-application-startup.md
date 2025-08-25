# 应用启动流程时序图

## 🚀 Application Startup Sequence

这个时序图展示了xiaozhi-esp32应用从启动到就绪的完整过程。

```plantuml
@startuml Application_Startup_Sequence
title xiaozhi-esp32 应用启动流程

participant "ESP-IDF" as IDF
participant "main()" as Main
participant "Application" as App
participant "Board" as Board
participant "AudioService" as Audio
participant "AudioCodec" as Codec
participant "Display" as Display
participant "Protocol" as Protocol
participant "OTA" as OTA
participant "MCP Server" as MCP

== 系统初始化阶段 ==
IDF -> Main: app_main()
activate Main

Main -> Main: esp_event_loop_create_default()
note right: 创建默认事件循环

Main -> Main: nvs_flash_init()
note right: 初始化NVS存储
alt NVS损坏
    Main -> Main: nvs_flash_erase()
    Main -> Main: nvs_flash_init()
end

== 应用启动阶段 ==
Main -> App: GetInstance()
activate App
App -> App: 创建单例实例
Main -> App: Start()

== 硬件初始化 ==
App -> App: SetDeviceState(kDeviceStateStarting)
App -> Board: GetInstance()
activate Board
Board -> Board: 根据板型创建具体实例
App -> Board: GetDisplay()
Board -> Display: 创建显示器实例
activate Display

== 音频系统初始化 ==
App -> Board: GetAudioCodec()
Board -> Codec: 创建音频编解码器
activate Codec
App -> Audio: Initialize(codec)
activate Audio
Audio -> Codec: Start()
Audio -> Audio: 创建Opus编解码器
Audio -> Audio: 配置音频处理器
Audio -> Audio: 配置唤醒词检测
App -> Audio: Start()
Audio -> Audio: 启动音频任务

== 回调函数设置 ==
App -> Audio: SetCallbacks()
note right: 设置音频事件回调
Audio -> App: on_send_queue_available
Audio -> App: on_wake_word_detected
Audio -> App: on_vad_change

== 网络初始化 ==
App -> Board: StartNetwork()
Board -> Board: 启动WiFi/以太网
App -> Display: UpdateStatusBar(true)

== 协议初始化 ==
App -> OTA: CheckNewVersion()
activate OTA
OTA -> OTA: 检查固件更新
OTA -> OTA: 获取配置信息
deactivate OTA

App -> MCP: AddCommonTools()
activate MCP
MCP -> MCP: 添加系统工具
deactivate MCP

alt MQTT配置
    App -> Protocol: 创建MqttProtocol
else WebSocket配置
    App -> Protocol: 创建WebsocketProtocol
else 默认
    App -> Protocol: 创建MqttProtocol (默认)
end

activate Protocol
App -> Protocol: OnNetworkError(callback)
App -> Protocol: OnIncomingAudio(callback)
App -> Protocol: Start()
Protocol -> Protocol: 建立网络连接

== 完成启动 ==
App -> App: SetDeviceState(kDeviceStateIdle)
App -> Display: ShowNotification("版本信息")
App -> Audio: PlaySound(成功音效)

== 进入主循环 ==
Main -> App: MainEventLoop()
loop 主事件循环
    App -> App: xEventGroupWaitBits()
    note right: 等待事件
    alt MAIN_EVENT_SCHEDULE
        App -> App: ProcessScheduledTasks()
    else MAIN_EVENT_WAKE_WORD_DETECTED
        App -> App: ProcessWakeWord()
    else MAIN_EVENT_SEND_AUDIO
        App -> Protocol: SendAudioPacket()
    else MAIN_EVENT_VAD_CHANGE
        App -> App: UpdateVoiceDetectionState()
    else MAIN_EVENT_ERROR
        App -> App: HandleNetworkError()
    end
end

deactivate Main
deactivate App
deactivate Board
deactivate Display
deactivate Codec
deactivate Audio
deactivate Protocol

@enduml
```

## 🔍 关键阶段说明

### 1. 系统初始化阶段 (0-100ms)
- ESP-IDF调用app_main()
- 创建事件循环和初始化NVS
- 包含错误恢复机制

### 2. 应用启动阶段 (100-500ms)
- 创建Application单例
- 设置设备状态为"启动中"
- 初始化硬件抽象层

### 3. 硬件初始化 (500-1000ms)
- 根据开发板类型创建具体Board实例
- 初始化显示器和音频编解码器
- 配置GPIO、I2C、SPI等外设

### 4. 音频系统初始化 (1000-1500ms)
- 创建AudioService和相关组件
- 启动音频采集和播放任务
- 配置唤醒词检测和语音处理

### 5. 网络和协议初始化 (1500-3000ms)
- 启动网络连接（WiFi/以太网）
- 检查OTA更新
- 根据配置选择通信协议
- 建立与服务器的连接

### 6. 完成启动 (3000ms+)
- 设置设备状态为"空闲"
- 播放成功提示音
- 进入主事件循环

## ⚡ 性能关键点

### 并行初始化
- 硬件初始化与网络连接并行进行
- 音频系统独立初始化，不阻塞主流程

### 错误处理
- NVS损坏自动恢复
- 网络连接失败重试机制
- 音频设备初始化失败降级处理

### 资源管理
- 使用智能指针自动管理内存
- RAII模式确保资源正确释放
- 事件驱动避免轮询浪费

---

**相关文档**:
- [Application类详细分析](../02-main-core/02-application-class.md)
- [音频服务启动流程](./02-audio-service-startup.md)
- [网络协议连接流程](./03-protocol-connection.md)
