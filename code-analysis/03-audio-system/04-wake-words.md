# 唤醒词检测 (WakeWord) 系统详解

## 📁 模块概览
- **文件数量**: 6个文件 (3个.h + 3个.cc)
- **功能**: 语音唤醒词检测，支持本地离线识别
- **设计模式**: 策略模式 + 观察者模式
- **核心实现**: EspWakeWord, AfeWakeWord, CustomWakeWord

## 🏗️ 架构设计

### 继承层次结构
```
WakeWord (抽象基类)
├── EspWakeWord          # ESP官方唤醒词检测
├── AfeWakeWord          # 基于AFE的唤醒词检测
└── CustomWakeWord       # 自定义唤醒词检测
```

## 🎯 WakeWord基类分析

### 核心接口定义
```cpp
class WakeWord {
public:
    virtual ~WakeWord() = default;

    // 生命周期管理
    virtual bool Initialize(AudioCodec* codec) = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;

    // 数据处理
    virtual void Feed(const std::vector<int16_t>& data) = 0;
    virtual size_t GetFeedSize() = 0;

    // 事件回调
    virtual void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) = 0;

    // 唤醒词数据管理
    virtual void EncodeWakeWordData() = 0;
    virtual bool GetWakeWordOpus(std::vector<uint8_t>& opus) = 0;
    virtual const std::string& GetLastDetectedWakeWord() const = 0;
};
```

### 接口设计特点

#### 1. 流式处理设计
- **Feed方法**: 接收音频流数据
- **GetFeedSize**: 返回期望的输入数据大小
- **实时响应**: 支持实时音频流处理

#### 2. 事件驱动模式
- **OnWakeWordDetected**: 检测到唤醒词时的回调
- **异步通知**: 不阻塞音频处理流程
- **上下文传递**: 包含具体的唤醒词内容

#### 3. 数据编码功能
- **EncodeWakeWordData**: 编码唤醒词音频
- **GetWakeWordOpus**: 获取Opus编码的唤醒词
- **数据上传**: 支持向服务器发送唤醒词片段

## 🔊 EspWakeWord 详细分析

### ESP WakeNet 架构
EspWakeWord基于ESP官方的WakeNet神经网络模型，提供高精度的唤醒词检测。

### 核心成员变量
```cpp
class EspWakeWord : public WakeWord {
private:
    // ESP WakeNet接口
    esp_wn_iface_t *wakenet_iface_ = nullptr;        // WakeNet接口
    model_iface_data_t *wakenet_data_ = nullptr;     // 模型数据
    srmodel_list_t *wakenet_model_ = nullptr;        // 语音识别模型列表

    // 系统组件
    AudioCodec* codec_ = nullptr;                    // 音频编解码器
    std::atomic<bool> running_ = false;              // 运行状态

    // 回调和状态
    std::function<void(const std::string& wake_word)> wake_word_detected_callback_;
    std::string last_detected_wake_word_;            // 最后检测到的唤醒词
};
```

### 初始化流程分析

#### 1. 模型加载
```cpp
bool EspWakeWord::Initialize(AudioCodec* codec) {
    codec_ = codec;

    // 获取WakeNet接口
    wakenet_iface_ = esp_wn_handle;
    if (wakenet_iface_ == nullptr) {
        ESP_LOGE(TAG, "Failed to get wakenet interface");
        return false;
    }

    // 加载唤醒词模型
    wakenet_model_ = esp_srmodel_init("model");
    if (wakenet_model_ == nullptr) {
        ESP_LOGE(TAG, "Failed to load wakenet model");
        return false;
    }

    // 创建WakeNet数据结构
    wakenet_data_ = wakenet_iface_->create(wakenet_model_, DET_MODE_95);
    if (wakenet_data_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create wakenet data");
        return false;
    }

    ESP_LOGI(TAG, "WakeNet initialized successfully");
    return true;
}
```

#### 2. 检测模式配置
```cpp
typedef enum {
    DET_MODE_90 = 0,    // 检测模式90%（更灵敏）
    DET_MODE_95,        // 检测模式95%（平衡）
    DET_MODE_99,        // 检测模式99%（更精确）
} det_mode_t;

// 默认使用95%平衡模式
wakenet_data_ = wakenet_iface_->create(wakenet_model_, DET_MODE_95);
```

### 实时检测流程

#### 1. Feed 方法实现
```cpp
void EspWakeWord::Feed(const std::vector<int16_t>& data) {
    if (!running_ || !wakenet_iface_ || !wakenet_data_) {
        return;
    }

    // 获取期望的输入帧大小
    int feed_chunk_size = wakenet_iface_->get_samp_chunksize(wakenet_data_);

    if (data.size() >= feed_chunk_size) {
        // 检测唤醒词
        int detection_result = wakenet_iface_->detect(
            wakenet_data_,
            const_cast<int16_t*>(data.data())
        );

        if (detection_result > 0) {
            // 检测到唤醒词
            ProcessWakeWordDetection(detection_result);
        }
    }
}
```

#### 2. 唤醒词处理
```cpp
void EspWakeWord::ProcessWakeWordDetection(int wake_word_id) {
    // 根据ID获取唤醒词字符串
    std::string wake_word = GetWakeWordById(wake_word_id);
    last_detected_wake_word_ = wake_word;

    ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());

    // 触发回调
    if (wake_word_detected_callback_) {
        wake_word_detected_callback_(wake_word);
    }

    // 开始编码唤醒词音频数据
    EncodeWakeWordData();
}
```

### 音频数据编码

#### 1. 唤醒词音频提取
```cpp
void EspWakeWord::EncodeWakeWordData() {
    if (!wakenet_iface_ || !wakenet_data_) {
        return;
    }

    // 获取唤醒词音频片段
    // 通常包含唤醒词前后各1-2秒的音频
    int wake_word_length = wakenet_iface_->get_wake_word_len(wakenet_data_);
    std::vector<int16_t> wake_word_audio(wake_word_length);

    int extracted = wakenet_iface_->get_wake_word_data(
        wakenet_data_,
        wake_word_audio.data()
    );

    if (extracted > 0) {
        // 使用Opus编码器编码音频
        EncodeToOpus(wake_word_audio);
    }
}
```

#### 2. Opus编码实现
```cpp
bool EspWakeWord::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    if (opus_encoded_data_.empty()) {
        return false;
    }

    opus = opus_encoded_data_;
    return true;
}

void EspWakeWord::EncodeToOpus(const std::vector<int16_t>& audio_data) {
    // 创建Opus编码器
    int error;
    OpusEncoder* encoder = opus_encoder_create(
        16000,          // 采样率
        1,              // 通道数
        OPUS_APPLICATION_VOIP,  // 语音应用
        &error
    );

    if (error != OPUS_OK) {
        ESP_LOGE(TAG, "Failed to create Opus encoder: %d", error);
        return;
    }

    // 设置编码参数
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(24000));  // 24kbps
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(5));   // 复杂度5

    // 编码音频数据
    const int frame_size = 320;  // 20ms @ 16kHz
    opus_encoded_data_.clear();

    for (size_t i = 0; i + frame_size <= audio_data.size(); i += frame_size) {
        uint8_t opus_frame[256];
        int encoded_bytes = opus_encode(
            encoder,
            &audio_data[i],
            frame_size,
            opus_frame,
            sizeof(opus_frame)
        );

        if (encoded_bytes > 0) {
            opus_encoded_data_.insert(
                opus_encoded_data_.end(),
                opus_frame,
                opus_frame + encoded_bytes
            );
        }
    }

    opus_encoder_destroy(encoder);
}
```

## 🔄 AfeWakeWord 分析

### 集成AFE处理器
AfeWakeWord将唤醒词检测集成到AFE音频处理流程中。

```cpp
class AfeWakeWord : public WakeWord {
private:
    esp_afe_sr_iface_t* afe_iface_ = nullptr;    // AFE接口
    esp_afe_sr_data_t* afe_data_ = nullptr;      // AFE数据

    // AFE中包含WakeNet
    bool wakenet_enabled_ = true;

public:
    bool Initialize(AudioCodec* codec) override {
        // 配置AFE包含WakeNet
        afe_config_t afe_config = {
            .wakenet_init = true,           // 启用WakeNet
            .wakenet_mode = DET_MODE_95,    // 检测模式
            .afe_mode = SR_MODE_LOW_COST,   // 低成本模式
            // ... 其他AFE配置
        };

        afe_iface_ = &ESP_AFE_SR_HANDLE;
        afe_data_ = afe_iface_->create_from_config(&afe_config);

        return afe_data_ != nullptr;
    }

    void Feed(const std::vector<int16_t>& data) override {
        if (!afe_iface_ || !afe_data_) return;

        // 送入AFE处理
        afe_iface_->feed(afe_data_, data.data());

        // 检查唤醒词检测结果
        int wake_word_id = afe_iface_->get_wakenet_id(afe_data_);
        if (wake_word_id > 0) {
            ProcessWakeWordDetection(wake_word_id);
        }
    }
};
```

## 🎛️ CustomWakeWord 分析

### 自定义模型支持
CustomWakeWord支持用户自定义的唤醒词模型。

```cpp
class CustomWakeWord : public WakeWord {
private:
    // 自定义模型接口
    void* custom_model_handle_ = nullptr;
    std::string model_path_;

    // 检测参数
    float detection_threshold_ = 0.95f;
    int window_size_ms_ = 1000;

public:
    bool LoadCustomModel(const std::string& model_path) {
        model_path_ = model_path;

        // 加载自定义模型文件
        custom_model_handle_ = LoadModelFromFile(model_path);
        return custom_model_handle_ != nullptr;
    }

    void SetDetectionThreshold(float threshold) {
        detection_threshold_ = threshold;
    }

    void Feed(const std::vector<int16_t>& data) override {
        if (!custom_model_handle_) return;

        // 使用自定义模型进行检测
        float confidence = RunInference(custom_model_handle_, data);

        if (confidence > detection_threshold_) {
            // 检测到唤醒词
            std::string wake_word = "custom_wake_word";
            ProcessWakeWordDetection(wake_word);
        }
    }
};
```

## ⚡ 性能优化

### 内存管理
```cpp
// 模型内存优化
- **模型压缩**: 使用量化和剪枝技术
- **内存复用**: 复用音频缓冲区
- **懒加载**: 按需加载模型数据
```

### 计算优化
```cpp
// CPU优化
- **SIMD指令**: 使用ESP32的SIMD加速
- **定点运算**: 避免浮点运算
- **流水线处理**: 并行音频处理和检测
```

### 功耗优化
```cpp
// 功耗管理
- **低功耗模式**: 检测时降低CPU频率
- **智能唤醒**: 基于VAD的智能激活
- **模型选择**: 根据场景选择不同复杂度模型
```

## 🔗 与其他模块集成

### AudioService集成
```cpp
void AudioService::InitializeWakeWord() {
    // 根据配置选择唤醒词检测器
    Settings settings("wake_word", true);
    std::string type = settings.GetString("type", "esp");

    if (type == "esp") {
        wake_word_ = std::make_unique<EspWakeWord>();
    } else if (type == "afe") {
        wake_word_ = std::make_unique<AfeWakeWord>();
    } else if (type == "custom") {
        wake_word_ = std::make_unique<CustomWakeWord>();
    }

    // 初始化和设置回调
    wake_word_->Initialize(audio_codec_);
    wake_word_->OnWakeWordDetected([this](const std::string& wake_word) {
        OnWakeWordDetected(wake_word);
    });
}
```

### 协议集成
```cpp
void AudioService::OnWakeWordDetected(const std::string& wake_word) {
    ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());

    // 编码唤醒词音频
    wake_word_->EncodeWakeWordData();

    // 发送到服务器
    std::vector<uint8_t> opus_data;
    if (wake_word_->GetWakeWordOpus(opus_data)) {
        protocol_->SendWakeWordDetected(wake_word);
        // 可选：发送唤醒词音频
        protocol_->SendWakeWordAudio(opus_data);
    }

    // 触发应用状态变化
    Application::GetInstance().OnWakeWordDetected(wake_word);
}
```

## 🎯 使用场景分析

### 1. 智能音箱场景
```cpp
// 高精度检测
EspWakeWord wake_word;
wake_word.Initialize(codec);
// 使用DET_MODE_99确保低误触发率
```

### 2. 语音助手场景
```cpp
// 平衡模式
AfeWakeWord wake_word;
// 集成音频处理和唤醒检测
```

### 3. 自定义应用场景
```cpp
// 特定领域唤醒词
CustomWakeWord wake_word;
wake_word.LoadCustomModel("/path/to/custom_model.bin");
wake_word.SetDetectionThreshold(0.9f);
```

## 🔧 配置和调试

### 模型配置
```cpp
// 模型文件路径配置
#define WAKENET_MODEL_PATH "/spiffs/wakenet_model.bin"
#define CUSTOM_MODEL_PATH "/spiffs/custom_wake_word.bin"

// 检测阈值配置
Settings settings("wake_word", true);
float threshold = settings.GetFloat("threshold", 0.95f);
int mode = settings.GetInt("detection_mode", DET_MODE_95);
```

### 调试工具
```cpp
// 唤醒词检测调试
class WakeWordDebugger {
public:
    void LogDetectionResult(int wake_word_id, float confidence) {
        ESP_LOGI(TAG, "Detection: id=%d, confidence=%.3f", wake_word_id, confidence);
    }

    void SaveWakeWordAudio(const std::vector<int16_t>& audio) {
        // 保存唤醒词音频用于分析
        WriteToFile("/debug/wake_word_audio.wav", audio);
    }
};
```

---

**相关文档**:
- [AudioService核心分析](./01-audio-service.md)
- [音频处理器详解](./03-audio-processors.md)
- [应用状态管理](../02-main-core/02-application-class.md)
