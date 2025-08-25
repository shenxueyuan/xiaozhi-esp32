# 通信协议 (Protocol) 系统详解

## 📁 模块概览
- **文件数量**: 6个文件 (3个.h + 3个.cc)
- **功能**: 与服务器通信，支持音频流和JSON消息
- **设计模式**: 策略模式 + 观察者模式 + 二进制协议
- **实现类型**: MQTT、WebSocket双协议支持

## 🏗️ 架构设计

### 继承层次结构
```
Protocol (抽象基类)
├── MqttProtocol          # MQTT + UDP混合协议
└── WebsocketProtocol     # WebSocket协议
```

## 🎯 Protocol基类分析

### 核心数据结构

#### 音频流数据包
```cpp
struct AudioStreamPacket {
    int sample_rate = 0;              // 采样率
    int frame_duration = 0;           // 帧时长(ms)
    uint32_t timestamp = 0;           // 时间戳
    std::vector<uint8_t> payload;     // Opus编码的音频数据
};
```

#### 二进制协议格式
```cpp
// 协议版本2 - WebSocket使用
struct BinaryProtocol2 {
    uint16_t version;                 // 协议版本号
    uint16_t type;                    // 消息类型 (0: OPUS, 1: JSON)
    uint32_t reserved;                // 保留字段
    uint32_t timestamp;               // 时间戳(用于服务端AEC)
    uint32_t payload_size;            // 载荷大小
    uint8_t payload[];                // 载荷数据
} __attribute__((packed));

// 协议版本3 - 紧凑格式
struct BinaryProtocol3 {
    uint8_t type;                     // 消息类型
    uint8_t reserved;                 // 保留字段
    uint16_t payload_size;            // 载荷大小
    uint8_t payload[];                // 载荷数据
} __attribute__((packed));
```

#### 枚举类型定义
```cpp
enum AbortReason {
    kAbortReasonNone,                 // 无中断
    kAbortReasonWakeWordDetected      // 检测到唤醒词
};

enum ListeningMode {
    kListeningModeAutoStop,           // 自动停止监听
    kListeningModeManualStop,         // 手动停止监听
    kListeningModeRealtime            // 实时模式 (需要AEC支持)
};
```

### 基类接口设计
```cpp
class Protocol {
public:
    virtual ~Protocol() = default;

    // 参数查询接口
    inline int server_sample_rate() const { return server_sample_rate_; }
    inline int server_frame_duration() const { return server_frame_duration_; }
    inline const std::string& session_id() const { return session_id_; }

    // 回调注册接口
    void OnIncomingAudio(std::function<void(std::unique_ptr<AudioStreamPacket> packet)> callback);
    void OnIncomingJson(std::function<void(const cJSON* root)> callback);
    void OnAudioChannelOpened(std::function<void()> callback);
    void OnAudioChannelClosed(std::function<void()> callback);
    void OnNetworkError(std::function<void(const std::string& message)> callback);

    // 纯虚函数 - 由子类实现
    virtual bool Start() = 0;
    virtual bool OpenAudioChannel() = 0;
    virtual void CloseAudioChannel() = 0;
    virtual bool IsAudioChannelOpened() const = 0;
    virtual bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) = 0;

    // 默认实现的消息发送
    virtual void SendWakeWordDetected(const std::string& wake_word);
    virtual void SendStartListening(ListeningMode mode);
    virtual void SendStopListening();
    virtual void SendAbortSpeaking(AbortReason reason);
    virtual void SendMcpMessage(const std::string& message);

protected:
    // 回调函数存储
    std::function<void(const cJSON* root)> on_incoming_json_;
    std::function<void(std::unique_ptr<AudioStreamPacket> packet)> on_incoming_audio_;
    std::function<void()> on_audio_channel_opened_;
    std::function<void()> on_audio_channel_closed_;
    std::function<void(const std::string& message)> on_network_error_;

    // 协议参数
    int server_sample_rate_ = 24000;          // 服务器采样率
    int server_frame_duration_ = 60;          // 服务器帧时长
    bool error_occurred_ = false;             // 错误标志
    std::string session_id_;                  // 会话ID

    // 超时检测
    std::chrono::time_point<std::chrono::steady_clock> last_incoming_time_;

    // 纯虚函数
    virtual bool SendText(const std::string& text) = 0;
    virtual void SetError(const std::string& message);
    virtual bool IsTimeout() const;
};
```

## 📡 MqttProtocol 详细分析

### MQTT + UDP 混合架构
MqttProtocol采用独特的混合通信架构：
- **MQTT**: 用于控制消息和JSON数据传输
- **UDP**: 用于实时音频流传输（低延迟）

### 核心成员变量
```cpp
class MqttProtocol : public Protocol {
private:
    // FreeRTOS事件组
    EventGroupHandle_t event_group_handle_;

    // MQTT配置
    std::string publish_topic_;               // 发布主题
    std::unique_ptr<Mqtt> mqtt_;              // MQTT客户端

    // UDP音频通道
    std::unique_ptr<Udp> udp_;                // UDP客户端
    std::string udp_server_;                  // UDP服务器地址
    int udp_port_;                            // UDP端口

    // 加密相关
    mbedtls_aes_context aes_ctx_;             // AES加密上下文
    std::string aes_nonce_;                   // AES随机数
    uint32_t local_sequence_;                 // 本地序列号
    uint32_t remote_sequence_;                // 远程序列号

    // 线程安全
    std::mutex channel_mutex_;

    // 常量定义
    static constexpr int MQTT_PING_INTERVAL_SECONDS = 90;
    static constexpr int MQTT_RECONNECT_INTERVAL_MS = 10000;
    static constexpr int MQTT_PROTOCOL_SERVER_HELLO_EVENT = (1 << 0);
};
```

### MQTT连接建立流程
```cpp
bool MqttProtocol::StartMqttClient(bool report_error) {
    // 1. 从配置读取MQTT参数
    Settings settings("mqtt", false);
    auto endpoint = settings.GetString("endpoint");
    auto client_id = settings.GetString("client_id");
    auto username = settings.GetString("username");
    auto password = settings.GetString("password");
    int keepalive_interval = settings.GetInt("keepalive", 240);
    publish_topic_ = settings.GetString("publish_topic");

    if (endpoint.empty()) {
        ESP_LOGW(TAG, "MQTT endpoint is not specified");
        if (report_error) {
            SetError("MQTT服务器未配置");
        }
        return false;
    }

    // 2. 创建MQTT客户端
    auto network = Board::GetInstance().GetNetwork();
    mqtt_ = network->CreateMqtt(0);
    mqtt_->SetKeepAlive(keepalive_interval);

    // 3. 设置MQTT事件回调
    mqtt_->OnDisconnected([this]() {
        ESP_LOGI(TAG, "MQTT disconnected");
        SetError("MQTT连接断开");
    });

    mqtt_->OnMessage([this](const std::string& topic, const std::string& payload) {
        // 处理收到的MQTT消息
        ProcessMqttMessage(topic, payload);
    });

    // 4. 连接到MQTT服务器
    if (!mqtt_->Connect(endpoint, client_id, username, password)) {
        ESP_LOGE(TAG, "Failed to connect to MQTT server");
        if (report_error) {
            SetError("无法连接到MQTT服务器");
        }
        return false;
    }

    return true;
}
```

### UDP音频通道建立
```cpp
bool MqttProtocol::OpenAudioChannel() {
    std::lock_guard<std::mutex> lock(channel_mutex_);

    // 1. 等待服务器Hello消息
    EventBits_t bits = xEventGroupWaitBits(
        event_group_handle_,
        MQTT_PROTOCOL_SERVER_HELLO_EVENT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(10000)  // 10秒超时
    );

    if (!(bits & MQTT_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Timeout waiting for server hello");
        return false;
    }

    // 2. 创建UDP连接
    auto network = Board::GetInstance().GetNetwork();
    udp_ = network->CreateUdp(1);

    if (!udp_->Connect(udp_server_, udp_port_)) {
        ESP_LOGE(TAG, "Failed to connect UDP: %s:%d", udp_server_.c_str(), udp_port_);
        return false;
    }

    // 3. 设置UDP数据接收回调
    udp_->OnData([this](const uint8_t* data, size_t length) {
        ProcessUdpAudioData(data, length);
    });

    // 4. 初始化AES加密
    mbedtls_aes_init(&aes_ctx_);
    // 使用从服务器Hello消息中获取的密钥
    mbedtls_aes_setkey_enc(&aes_ctx_, aes_key_.data(), 128);

    ESP_LOGI(TAG, "Audio channel opened successfully");
    if (on_audio_channel_opened_) {
        on_audio_channel_opened_();
    }

    return true;
}
```

### 音频数据发送
```cpp
bool MqttProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    if (!udp_ || !udp_->IsConnected()) {
        return false;
    }

    // 1. 构建二进制协议包
    size_t total_size = sizeof(BinaryProtocol3) + packet->payload.size();
    std::vector<uint8_t> buffer(total_size);

    BinaryProtocol3* protocol = reinterpret_cast<BinaryProtocol3*>(buffer.data());
    protocol->type = 0;  // OPUS音频类型
    protocol->reserved = 0;
    protocol->payload_size = htons(packet->payload.size());

    // 2. 复制音频数据
    std::memcpy(protocol->payload, packet->payload.data(), packet->payload.size());

    // 3. AES加密 (可选)
    if (aes_enabled_) {
        EncryptAudioData(buffer.data(), total_size);
    }

    // 4. 通过UDP发送
    bool success = udp_->Send(buffer.data(), total_size);
    if (success) {
        local_sequence_++;
    }

    return success;
}
```

## 🌐 WebsocketProtocol 分析

### WebSocket协议特点
- **全双工通信**: 同时支持音频和JSON消息
- **二进制协议**: 使用BinaryProtocol2格式
- **认证支持**: Bearer Token认证
- **协议版本**: 支持多版本协议

### 核心实现
```cpp
class WebsocketProtocol : public Protocol {
private:
    EventGroupHandle_t event_group_handle_;
    std::unique_ptr<WebSocket> websocket_;    // WebSocket客户端
    int version_ = 1;                         // 协议版本

    static constexpr int WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT = (1 << 0);

public:
    WebsocketProtocol();
    ~WebsocketProtocol();

    bool Start() override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel() override;
    bool IsAudioChannelOpened() const override;

private:
    void ParseServerHello(const cJSON* root);
    bool SendText(const std::string& text) override;
    std::string GetHelloMessage();
};
```

### WebSocket连接建立
```cpp
bool WebsocketProtocol::OpenAudioChannel() {
    // 1. 从配置读取参数
    Settings settings("websocket", false);
    std::string url = settings.GetString("url");
    std::string token = settings.GetString("token");
    int version = settings.GetInt("version");
    if (version != 0) {
        version_ = version;
    }

    error_occurred_ = false;

    // 2. 创建WebSocket连接
    auto network = Board::GetInstance().GetNetwork();
    websocket_ = network->CreateWebSocket(1);
    if (websocket_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create websocket");
        return false;
    }

    // 3. 设置HTTP头
    if (!token.empty()) {
        // 如果token没有空格，添加"Bearer "前缀
        if (token.find(" ") == std::string::npos) {
            token = "Bearer " + token;
        }
        websocket_->SetHeader("Authorization", token.c_str());
    }
    websocket_->SetHeader("Protocol-Version", std::to_string(version_).c_str());
    websocket_->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    websocket_->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());

    // 4. 设置数据接收回调
    websocket_->OnData([this](const char* data, size_t len, bool binary) {
        if (binary) {
            ProcessBinaryMessage(reinterpret_cast<const uint8_t*>(data), len);
        } else {
            ProcessTextMessage(std::string(data, len));
        }
    });

    // 5. 建立连接
    if (!websocket_->Connect(url)) {
        ESP_LOGE(TAG, "Failed to connect to WebSocket server");
        return false;
    }

    return true;
}
```

### 二进制消息处理
```cpp
void WebsocketProtocol::ProcessBinaryMessage(const uint8_t* data, size_t length) {
    if (length < sizeof(BinaryProtocol2)) {
        ESP_LOGW(TAG, "Binary message too short: %zu bytes", length);
        return;
    }

    const BinaryProtocol2* protocol = reinterpret_cast<const BinaryProtocol2*>(data);

    uint16_t version = ntohs(protocol->version);
    uint16_t type = ntohs(protocol->type);
    uint32_t timestamp = ntohl(protocol->timestamp);
    uint32_t payload_size = ntohl(protocol->payload_size);

    if (length < sizeof(BinaryProtocol2) + payload_size) {
        ESP_LOGW(TAG, "Invalid payload size: %u", payload_size);
        return;
    }

    if (type == 0) {  // OPUS音频数据
        // 创建音频数据包
        auto packet = std::make_unique<AudioStreamPacket>();
        packet->sample_rate = server_sample_rate_;
        packet->frame_duration = server_frame_duration_;
        packet->timestamp = timestamp;
        packet->payload.assign(protocol->payload,
                              protocol->payload + payload_size);

        // 通知上层应用
        if (on_incoming_audio_) {
            on_incoming_audio_(std::move(packet));
        }
    } else if (type == 1) {  // JSON消息
        std::string json_str(reinterpret_cast<const char*>(protocol->payload), payload_size);
        ProcessJsonMessage(json_str);
    }
}
```

### 音频发送实现
```cpp
bool WebsocketProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    if (!IsAudioChannelOpened()) {
        return false;
    }

    // 1. 构建二进制协议包
    size_t total_size = sizeof(BinaryProtocol2) + packet->payload.size();
    std::vector<uint8_t> buffer(total_size);

    BinaryProtocol2* protocol = reinterpret_cast<BinaryProtocol2*>(buffer.data());
    protocol->version = htons(version_);
    protocol->type = htons(0);  // OPUS类型
    protocol->reserved = 0;
    protocol->timestamp = htonl(packet->timestamp);
    protocol->payload_size = htonl(packet->payload.size());

    // 2. 复制音频数据
    std::memcpy(protocol->payload, packet->payload.data(), packet->payload.size());

    // 3. 通过WebSocket发送二进制数据
    return websocket_->SendBinary(buffer.data(), total_size);
}
```

## 🔄 协议切换机制

### 协议工厂模式
```cpp
class ProtocolFactory {
public:
    static std::unique_ptr<Protocol> CreateProtocol(const std::string& type) {
        if (type == "mqtt") {
            return std::make_unique<MqttProtocol>();
        } else if (type == "websocket") {
            return std::make_unique<WebsocketProtocol>();
        } else {
            ESP_LOGE(TAG, "Unknown protocol type: %s", type.c_str());
            return nullptr;
        }
    }
};

// 使用示例
Settings settings("protocol", true);
std::string protocol_type = settings.GetString("type", "websocket");
auto protocol = ProtocolFactory::CreateProtocol(protocol_type);
```

### 自动故障切换
```cpp
class ProtocolManager {
private:
    std::vector<std::string> protocol_priorities_ = {"websocket", "mqtt"};
    size_t current_protocol_index_ = 0;
    std::unique_ptr<Protocol> current_protocol_;

public:
    bool ConnectWithFallback() {
        for (size_t i = 0; i < protocol_priorities_.size(); ++i) {
            current_protocol_index_ = i;
            std::string protocol_type = protocol_priorities_[i];

            current_protocol_ = ProtocolFactory::CreateProtocol(protocol_type);
            if (current_protocol_ && current_protocol_->Start()) {
                ESP_LOGI(TAG, "Connected using protocol: %s", protocol_type.c_str());
                return true;
            }

            ESP_LOGW(TAG, "Failed to connect using protocol: %s", protocol_type.c_str());
        }

        ESP_LOGE(TAG, "All protocols failed to connect");
        return false;
    }
};
```

## 🔐 安全特性

### 认证机制
```cpp
// WebSocket Bearer Token认证
websocket_->SetHeader("Authorization", "Bearer " + token);

// MQTT用户名密码认证
mqtt_->Connect(endpoint, client_id, username, password);
```

### 数据加密
```cpp
// AES-128加密音频数据
void MqttProtocol::EncryptAudioData(uint8_t* data, size_t length) {
    mbedtls_aes_crypt_cbc(&aes_ctx_, MBEDTLS_AES_ENCRYPT,
                         length, aes_nonce_.data(),
                         data, data);
}
```

### 序列号防重放
```cpp
// 检查远程序列号防止重放攻击
bool MqttProtocol::ValidateSequence(uint32_t sequence) {
    if (sequence <= remote_sequence_) {
        ESP_LOGW(TAG, "Invalid sequence number: %u (expected > %u)",
                 sequence, remote_sequence_);
        return false;
    }
    remote_sequence_ = sequence;
    return true;
}
```

## ⚡ 性能优化

### UDP音频优化
```cpp
// UDP专用于音频流传输
- 零拷贝发送
- 最小协议头开销
- 无重传机制（实时性优先）
- 自定义拥塞控制
```

### WebSocket优化
```cpp
// 二进制协议减少开销
- 网络字节序转换
- 紧凑的协议头
- 批量消息处理
- 异步收发分离
```

### 内存管理
```cpp
// 智能指针管理音频包
std::unique_ptr<AudioStreamPacket> packet;

// 预分配缓冲区
std::vector<uint8_t> buffer(total_size);

// 移动语义避免拷贝
on_incoming_audio_(std::move(packet));
```

---

**相关文档**:
- [音频系统集成](../03-audio-system/01-audio-service.md)
- [网络接口实现](./02-network-interfaces.md)
- [应用状态管理](../02-main-core/02-application-class.md)
