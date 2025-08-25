# Application 类 - 应用主控制器

## 📁 文件信息
- **路径**: `main/application.h` + `main/application.cc`
- **类型**: 核心控制类
- **设计模式**: 单例模式
- **功能**: 整个应用程序的主控制器和协调中心

## 🎯 核心功能

### 类定义概览
```cpp
class Application {
public:
    static Application& GetInstance();

    void Start();                    // 启动应用
    void MainEventLoop();           // 主事件循环
    DeviceState GetDeviceState() const;
    void SetDeviceState(DeviceState state);
    void Schedule(std::function<void()> callback);
    void ToggleChatState();
    // ... 更多方法

private:
    Application();                   // 私有构造函数
    ~Application();

    std::mutex mutex_;
    std::deque<std::function<void()>> main_tasks_;
    std::unique_ptr<Protocol> protocol_;
    AudioService audio_service_;
    // ... 更多成员
};
```

## 🏗️ 架构设计

### 单例模式实现
```cpp
static Application& GetInstance() {
    static Application instance;
    return instance;
}
```

### 核心子系统
1. **AudioService** - 音频处理服务
2. **Protocol** - 通信协议（MQTT/WebSocket）
3. **Board** - 硬件抽象层
4. **Display** - 显示系统

## 🔗 关键依赖

### 主要包含文件
```cpp
#include "board.h"              // 硬件抽象
#include "display.h"            // 显示系统
#include "audio_service.h"      // 音频服务
#include "mqtt_protocol.h"      // MQTT协议
#include "websocket_protocol.h" // WebSocket协议
#include "mcp_server.h"         // MCP服务器
```

### 调用关系图
```
main()
  └─→ Application::GetInstance()
      └─→ Application::Start()
          ├─→ Board::GetInstance()
          ├─→ AudioService::Initialize()
          ├─→ Protocol::Start()
          └─→ Application::MainEventLoop()
```

## 📋 启动流程分析

### Start() 方法详解
```cpp
void Application::Start() {
    auto& board = Board::GetInstance();
    SetDeviceState(kDeviceStateStarting);

    /* 1. 设置显示器 */
    auto display = board.GetDisplay();

    /* 2. 设置音频服务 */
    auto codec = board.GetAudioCodec();
    audio_service_.Initialize(codec);
    audio_service_.Start();

    /* 3. 设置回调函数 */
    AudioServiceCallbacks callbacks;
    // ... 设置各种回调

    /* 4. 启动网络 */
    board.StartNetwork();

    /* 5. 初始化协议 */
    if (ota.HasMqttConfig()) {
        protocol_ = std::make_unique<MqttProtocol>();
    } else if (ota.HasWebsocketConfig()) {
        protocol_ = std::make_unique<WebsocketProtocol>();
    }

    /* 6. 设置设备状态为空闲 */
    SetDeviceState(kDeviceStateIdle);
}
```

## 🔄 事件循环机制

### MainEventLoop() 核心
```cpp
void Application::MainEventLoop() {
    while (true) {
        EventBits_t bits = xEventGroupWaitBits(event_group_,
            MAIN_EVENT_SCHEDULE | MAIN_EVENT_SEND_AUDIO |
            MAIN_EVENT_WAKE_WORD_DETECTED | MAIN_EVENT_VAD_CHANGE |
            MAIN_EVENT_ERROR, pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & MAIN_EVENT_SCHEDULE) {
            ProcessScheduledTasks();
        }
        if (bits & MAIN_EVENT_WAKE_WORD_DETECTED) {
            ProcessWakeWord();
        }
        // ... 处理其他事件
    }
}
```

### 事件类型
```cpp
#define MAIN_EVENT_SCHEDULE (1 << 0)           // 调度任务
#define MAIN_EVENT_SEND_AUDIO (1 << 1)         // 发送音频
#define MAIN_EVENT_WAKE_WORD_DETECTED (1 << 2) // 唤醒词检测
#define MAIN_EVENT_VAD_CHANGE (1 << 3)         // 语音活动检测
#define MAIN_EVENT_ERROR (1 << 4)              // 错误处理
#define MAIN_EVENT_CHECK_NEW_VERSION_DONE (1 << 5) // OTA检查完成
```

## 🎛️ 设备状态管理

### 设备状态枚举
```cpp
enum DeviceState {
    kDeviceStateUnknown,
    kDeviceStateStarting,      // 启动中
    kDeviceStateConfiguring,   // 配置中
    kDeviceStateIdle,          // 空闲
    kDeviceStateConnecting,    // 连接中
    kDeviceStateListening,     // 监听中
    kDeviceStateSpeaking,      // 说话中
    kDeviceStateUpgrading,     // 升级中
    kDeviceStateActivating,    // 激活中
    kDeviceStateAudioTesting,  // 音频测试
    kDeviceStateFatalError,    // 致命错误
};
```

### 状态转换方法
```cpp
void Application::SetDeviceState(DeviceState state) {
    if (device_state_ == state) return;

    ESP_LOGI(TAG, "State change: %s -> %s",
             STATE_STRINGS[device_state_],
             STATE_STRINGS[state]);

    device_state_ = state;
    // 通知显示器更新状态
    auto display = Board::GetInstance().GetDisplay();
    display->SetDeviceState(state);
}
```

## 🧵 任务调度机制

### Schedule() 方法
```cpp
void Application::Schedule(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, MAIN_EVENT_SCHEDULE);
}
```

### 线程安全设计
- 使用`std::mutex`保护任务队列
- 使用FreeRTOS事件组通知事件循环
- 支持跨任务的异步调用

## 🎵 音频处理集成

### 音频服务回调
```cpp
AudioServiceCallbacks callbacks;
callbacks.on_send_queue_available = [this]() {
    xEventGroupSetBits(event_group_, MAIN_EVENT_SEND_AUDIO);
};
callbacks.on_wake_word_detected = [this](const std::string& wake_word) {
    xEventGroupSetBits(event_group_, MAIN_EVENT_WAKE_WORD_DETECTED);
};
callbacks.on_vad_change = [this](bool speaking) {
    xEventGroupSetBits(event_group_, MAIN_EVENT_VAD_CHANGE);
};
```

## 🌐 网络通信集成

### 协议选择逻辑
```cpp
if (ota.HasMqttConfig()) {
    protocol_ = std::make_unique<MqttProtocol>();
} else if (ota.HasWebsocketConfig()) {
    protocol_ = std::make_unique<WebsocketProtocol>();
} else {
    ESP_LOGW(TAG, "No protocol specified, using MQTT");
    protocol_ = std::make_unique<MqttProtocol>();
}
```

### 协议事件处理
```cpp
protocol_->OnNetworkError([this](const std::string& message) {
    last_error_message_ = message;
    xEventGroupSetBits(event_group_, MAIN_EVENT_ERROR);
});

protocol_->OnIncomingAudio([this](std::unique_ptr<AudioStreamPacket> packet) {
    audio_service_.PushPacketToDecodeQueue(std::move(packet));
});
```

## 🔧 性能优化特性

### 内存管理
- 使用智能指针管理资源
- RAII设计模式确保资源正确释放
- 避免内存泄漏

### 并发控制
- 事件驱动的异步架构
- 最小化锁的使用范围
- 使用FreeRTOS原语提高效率

---

**相关文档**:
- [音频服务分析](../03-audio-system/01-audio-service.md)
- [硬件抽象层](../05-board-abstraction/01-board-base.md)
- [通信协议](../06-protocols/01-protocol-base.md)
