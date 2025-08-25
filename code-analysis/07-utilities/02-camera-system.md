# 摄像头系统 (Camera) 详解

## 📁 模块概览
- **功能**: 图像采集和处理，支持多种摄像头型号
- **设计模式**: 工厂模式 + RAII模式
- **支持型号**: OV2640, OV3660, GC0308等
- **核心实现**: Esp32Camera类

## 🏗️ 架构设计

### 继承层次结构
```
Camera (抽象基类 - 在Board中定义)
├── Esp32Camera          # ESP32-CAM摄像头实现
├── SscmaCamera          # SenseCAP摄像头实现
└── NoCamera (默认)      # 无摄像头实现
```

## 🎯 Esp32Camera核心分析

### 核心成员变量
```cpp
class Esp32Camera {
private:
    camera_fb_t* fb_ = nullptr;           // 摄像头帧缓冲区
    lv_img_dsc_t preview_image_;          // LVGL预览图像描述符
    std::string explain_url_;             // 图像解释服务URL
    std::string explain_token_;           // 认证令牌
    std::thread encoder_thread_;          // 编码线程

public:
    Esp32Camera(const camera_config_t& config);
    ~Esp32Camera();

    // 摄像头控制
    bool Capture();                       // 捕获图像
    void SetHMirror(bool enable);         // 水平镜像
    void SetVFlip(bool enable);           // 垂直翻转
    void SetExplainUrl(const std::string& url, const std::string& token);

    // 图像获取
    const lv_img_dsc_t* GetPreviewImage() const;
    camera_fb_t* GetFrameBuffer() const;
};
```

### 摄像头初始化详解

#### 硬件配置结构
```cpp
typedef struct {
    // 控制引脚
    int pin_pwdn;                    // 电源控制引脚
    int pin_reset;                   // 复位引脚
    int pin_xclk;                    // 外部时钟引脚

    // I2C控制引脚 (SCCB协议)
    int pin_sccb_sda;               // I2C数据线
    int pin_sccb_scl;               // I2C时钟线
    int sccb_i2c_port;              // I2C端口号

    // 并行数据引脚 (8位数据总线)
    int pin_d7;                     // 数据位7 (MSB)
    int pin_d6;                     // 数据位6
    int pin_d5;                     // 数据位5
    int pin_d4;                     // 数据位4
    int pin_d3;                     // 数据位3
    int pin_d2;                     // 数据位2
    int pin_d1;                     // 数据位1
    int pin_d0;                     // 数据位0 (LSB)

    // 同步信号引脚
    int pin_vsync;                  // 垂直同步
    int pin_href;                   // 水平参考/行同步
    int pin_pclk;                   // 像素时钟

    // 时钟配置
    int xclk_freq_hz;               // 外部时钟频率
    ledc_timer_t ledc_timer;        // LEDC定时器
    ledc_channel_t ledc_channel;    // LEDC通道

    // 图像格式配置
    pixformat_t pixel_format;       // 像素格式 (RGB565, JPEG等)
    framesize_t frame_size;         // 帧大小
    int jpeg_quality;               // JPEG质量 (0-63)

    // 缓冲区配置
    int fb_count;                   // 帧缓冲区数量
    camera_fb_location_t fb_location; // 缓冲区位置 (PSRAM/DRAM)
    camera_grab_mode_t grab_mode;   // 采集模式
} camera_config_t;
```

#### 不同开发板的配置示例

##### ESP-SparkBot配置
```cpp
void InitializeCamera() {
    camera_config_t camera_config = {};

    // 控制引脚配置
    camera_config.pin_pwdn = SPARKBOT_CAMERA_PWDN;    // GPIO_NUM_NC
    camera_config.pin_reset = SPARKBOT_CAMERA_RESET;  // GPIO_NUM_NC
    camera_config.pin_xclk = SPARKBOT_CAMERA_XCLK;    // GPIO_NUM_15

    // I2C配置
    camera_config.pin_sccb_sda = SPARKBOT_CAMERA_SIOD; // GPIO_NUM_4
    camera_config.pin_sccb_scl = SPARKBOT_CAMERA_SIOC; // GPIO_NUM_5
    camera_config.sccb_i2c_port = I2C_NUM_0;

    // 8位并行数据总线
    camera_config.pin_d0 = SPARKBOT_CAMERA_D0;        // GPIO_NUM_11
    camera_config.pin_d1 = SPARKBOT_CAMERA_D1;        // GPIO_NUM_9
    camera_config.pin_d2 = SPARKBOT_CAMERA_D2;        // GPIO_NUM_8
    camera_config.pin_d3 = SPARKBOT_CAMERA_D3;        // GPIO_NUM_10
    camera_config.pin_d4 = SPARKBOT_CAMERA_D4;        // GPIO_NUM_12
    camera_config.pin_d5 = SPARKBOT_CAMERA_D5;        // GPIO_NUM_18
    camera_config.pin_d6 = SPARKBOT_CAMERA_D6;        // GPIO_NUM_17
    camera_config.pin_d7 = SPARKBOT_CAMERA_D7;        // GPIO_NUM_16

    // 同步信号
    camera_config.pin_vsync = SPARKBOT_CAMERA_VSYNC;   // GPIO_NUM_6
    camera_config.pin_href = SPARKBOT_CAMERA_HSYNC;    // GPIO_NUM_7
    camera_config.pin_pclk = SPARKBOT_CAMERA_PCLK;     // GPIO_NUM_13

    // 时钟配置
    camera_config.xclk_freq_hz = SPARKBOT_CAMERA_XCLK_FREQ; // 20MHz
    camera_config.ledc_timer = SPARKBOT_LEDC_TIMER;     // LEDC_TIMER_0
    camera_config.ledc_channel = SPARKBOT_LEDC_CHANNEL; // LEDC_CHANNEL_0

    // 图像格式
    camera_config.pixel_format = PIXFORMAT_RGB565;      // RGB565格式
    camera_config.frame_size = FRAMESIZE_240X240;       // 240x240分辨率
    camera_config.jpeg_quality = 12;                    // JPEG质量
    camera_config.fb_count = 1;                         // 单缓冲
    camera_config.fb_location = CAMERA_FB_IN_PSRAM;     // PSRAM存储
    camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;   // 空闲时采集

    camera_ = new Esp32Camera(camera_config);
}
```

##### 正点原子配置 (特殊电源控制)
```cpp
void InitializeCamera() {
    // XL9555 GPIO扩展器控制摄像头电源
    xl9555_->SetOutputState(OV_PWDN_IO, 0);  // PWDN=低 (上电)
    xl9555_->SetOutputState(OV_RESET_IO, 0); // 确保复位
    vTaskDelay(pdMS_TO_TICKS(50));            // 延长复位保持时间
    xl9555_->SetOutputState(OV_RESET_IO, 1); // 释放复位
    vTaskDelay(pdMS_TO_TICKS(50));            // 延长50ms

    camera_config_t config = {};
    // ... 标准配置
    config.xclk_freq_hz = 24000000;          // 24MHz时钟
    config.frame_size = FRAMESIZE_QVGA;      // 320x240分辨率
    config.fb_location = CAMERA_FB_IN_PSRAM; // PSRAM缓冲

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        camera_ = nullptr;
        return;
    }

    camera_ = new Esp32Camera(config);
}
```

### 图像采集流程

#### Capture方法实现
```cpp
bool Esp32Camera::Capture() {
    // 等待编码线程完成
    if (encoder_thread_.joinable()) {
        encoder_thread_.join();
    }

    // 获取稳定帧 - 抛弃前几帧
    int frames_to_get = 2;
    for (int i = 0; i < frames_to_get; i++) {
        if (fb_ != nullptr) {
            esp_camera_fb_return(fb_);  // 释放前一帧
        }

        fb_ = esp_camera_fb_get();      // 获取新帧
        if (fb_ == nullptr) {
            ESP_LOGE(TAG, "Camera capture failed");
            return false;
        }
    }

    // 检查预览缓冲区
    if (preview_image_.data_size == 0 || preview_image_.data == nullptr) {
        ESP_LOGW(TAG, "Skip preview because of unsupported frame size");
        return true;  // 图像仍可上传服务器
    }

    // 显示预览图片
    UpdatePreviewImage();
    return true;
}
```

#### 预览图像处理
```cpp
void Esp32Camera::UpdatePreviewImage() {
    auto display = Board::GetInstance().GetDisplay();
    if (display == nullptr) return;

    // 从帧缓冲区复制RGB565数据到预览缓冲区
    auto src = (uint16_t*)fb_->buf;
    auto dst = (uint16_t*)preview_image_.data;

    size_t pixel_count = preview_image_.header.w * preview_image_.header.h;

    // 逐像素复制和格式转换
    for (size_t i = 0; i < pixel_count; i++) {
        dst[i] = src[i];  // RGB565直接复制
    }

    // 更新显示预览
    display->SetPreviewImage(&preview_image_);
}
```

### 传感器控制功能

#### 镜像和翻转控制
```cpp
void Esp32Camera::SetHMirror(bool enable) {
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor != nullptr) {
        sensor->set_hmirror(sensor, enable ? 1 : 0);
        ESP_LOGI(TAG, "Horizontal mirror: %s", enable ? "enabled" : "disabled");
    }
}

void Esp32Camera::SetVFlip(bool enable) {
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor != nullptr) {
        sensor->set_vflip(sensor, enable ? 1 : 0);
        ESP_LOGI(TAG, "Vertical flip: %s", enable ? "enabled" : "disabled");
    }
}
```

#### 特殊传感器处理
```cpp
Esp32Camera::Esp32Camera(const camera_config_t& config) {
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return;
    }

    // 获取传感器信息
    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == GC0308_PID) {
        // GC0308特殊处理
        s->set_hmirror(s, 0);  // 关闭镜像
        ESP_LOGI(TAG, "GC0308 sensor detected and configured");
    } else if (s->id.PID == OV2640_PID) {
        // OV2640特殊处理
        s->set_hmirror(s, 1);  // 启用镜像
        ESP_LOGI(TAG, "OV2640 sensor detected and configured");
    }

    InitializePreviewBuffer(config.frame_size);
}
```

### 内存管理优化

#### 预览缓冲区分配
```cpp
void Esp32Camera::InitializePreviewBuffer(framesize_t frame_size) {
    // 初始化LVGL图像描述符
    memset(&preview_image_, 0, sizeof(preview_image_));
    preview_image_.header.magic = LV_IMAGE_HEADER_MAGIC;
    preview_image_.header.cf = LV_COLOR_FORMAT_RGB565;
    preview_image_.header.flags = LV_IMAGE_FLAGS_ALLOCATED | LV_IMAGE_FLAGS_MODIFIABLE;

    // 根据帧大小设置分辨率
    switch (frame_size) {
        case FRAMESIZE_SVGA:    // 800x600
            preview_image_.header.w = 800;
            preview_image_.header.h = 600;
            break;
        case FRAMESIZE_VGA:     // 640x480
            preview_image_.header.w = 640;
            preview_image_.header.h = 480;
            break;
        case FRAMESIZE_QVGA:    // 320x240
            preview_image_.header.w = 320;
            preview_image_.header.h = 240;
            break;
        case FRAMESIZE_240X240: // 240x240 (正方形)
            preview_image_.header.w = 240;
            preview_image_.header.h = 240;
            break;
        case FRAMESIZE_128X128: // 128x128 (小正方形)
            preview_image_.header.w = 128;
            preview_image_.header.h = 128;
            break;
        default:
            ESP_LOGE(TAG, "Unsupported frame size: %d", frame_size);
            return;
    }

    // 计算缓冲区大小
    preview_image_.header.stride = preview_image_.header.w * 2; // RGB565 = 2字节/像素
    preview_image_.data_size = preview_image_.header.w * preview_image_.header.h * 2;

    // 分配PSRAM内存用于预览
    preview_image_.data = (uint8_t*)heap_caps_malloc(
        preview_image_.data_size,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    if (preview_image_.data == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for preview image",
                 preview_image_.data_size);
        preview_image_.data_size = 0;
    } else {
        ESP_LOGI(TAG, "Preview buffer allocated: %dx%d, %zu bytes",
                 preview_image_.header.w, preview_image_.header.h,
                 preview_image_.data_size);
    }
}
```

#### 内存释放机制
```cpp
Esp32Camera::~Esp32Camera() {
    // 等待编码线程完成
    if (encoder_thread_.joinable()) {
        encoder_thread_.join();
    }

    // 释放帧缓冲区
    if (fb_) {
        esp_camera_fb_return(fb_);
        fb_ = nullptr;
    }

    // 释放预览缓冲区
    if (preview_image_.data) {
        heap_caps_free((void*)preview_image_.data);
        preview_image_.data = nullptr;
        preview_image_.data_size = 0;
    }

    // 反初始化摄像头
    esp_camera_deinit();
    ESP_LOGI(TAG, "Camera deinitialized and memory released");
}
```

## 🔧 MCP工具集成

### 摄像头控制工具
```cpp
void EspSparkBot::InitializeTools() {
    auto& mcp_server = McpServer::GetInstance();

    // 拍照工具
    mcp_server.AddTool("take_photo", "拍摄照片并显示预览",
        PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            auto camera = GetCamera();
            if (camera && camera->Capture()) {
                return std::string("照片拍摄成功，预览已更新");
            } else {
                return std::string("拍照失败，请检查摄像头连接");
            }
        });

    // 镜像控制工具
    mcp_server.AddTool("set_camera_mirror", "设置摄像头镜像",
        PropertyList({
            Property("horizontal", kPropertyTypeBoolean, false),
            Property("vertical", kPropertyTypeBoolean, false)
        }), [this](const PropertyList& properties) -> ReturnValue {
            auto camera = GetCamera();
            if (camera) {
                bool h_mirror = properties["horizontal"].value<bool>();
                bool v_flip = properties["vertical"].value<bool>();

                camera->SetHMirror(h_mirror);
                camera->SetVFlip(v_flip);

                return std::string("镜像设置已更新");
            }
            return std::string("摄像头不可用");
        });
}
```

## ⚡ 性能优化

### 帧率优化
```cpp
// 时钟配置优化
config.xclk_freq_hz = 20000000;          // 20MHz获得更好的帧率
config.fb_count = 2;                     // 双缓冲提高流畅度
config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // 非阻塞采集

// 分辨率平衡
FRAMESIZE_240X240: 适合实时预览，计算量小
FRAMESIZE_QVGA:    平衡画质和性能
FRAMESIZE_VGA:     高画质，适合拍照
```

### 内存优化
```cpp
// PSRAM使用策略
config.fb_location = CAMERA_FB_IN_PSRAM;  // 帧缓冲使用PSRAM
preview_image_.data = heap_caps_malloc(   // 预览缓冲使用PSRAM
    size, MALLOC_CAP_SPIRAM
);

// 缓冲区复用
static uint8_t s_preview_buffer[240*240*2]; // 静态分配避免碎片
```

### 功耗控制
```cpp
void EspSparkBot::SetCameraPowerSaveMode(bool enable) {
    if (enable) {
        // 降低时钟频率
        sensor_t* sensor = esp_camera_sensor_get();
        sensor->set_quality(sensor, 20);  // 降低画质

        // 减少帧缓冲
        config.fb_count = 1;
    } else {
        // 恢复正常模式
        sensor->set_quality(sensor, 12);
        config.fb_count = 2;
    }
}
```

## 🔍 故障诊断

### 常见问题

#### 1. 摄像头初始化失败
```cpp
E (1234) Esp32Camera: Camera init failed with error 0x105
```
**原因**: 硬件连接问题或电源不足
**解决**: 检查引脚连接，确认电源供应

#### 2. 内存分配失败
```cpp
E (2345) Esp32Camera: Failed to allocate memory for preview image
```
**原因**: PSRAM不足或未启用
**解决**: 检查PSRAM配置，减少缓冲区大小

#### 3. 图像质量问题
```cpp
W (3456) Esp32Camera: Frame capture timeout
```
**原因**: 时钟频率不匹配或信号干扰
**解决**: 调整XCLK频率，检查信号完整性

### 调试工具
```cpp
// 摄像头状态检测
void DiagnosticCamera() {
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        ESP_LOGI(TAG, "Sensor PID: 0x%02X", sensor->id.PID);
        ESP_LOGI(TAG, "Sensor VER: 0x%02X", sensor->id.VER);
    } else {
        ESP_LOGE(TAG, "No sensor detected");
    }
}

// 内存使用监控
void MonitorCameraMemory() {
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "Free PSRAM: %zu bytes", free_psram);
}
```

---

**相关文档**:
- [Board硬件抽象层](../05-board-abstraction/01-board-base.md)
- [开发板实现详解](../05-board-abstraction/02-board-implementations.md)
- [MCP工具系统](./03-mcp-server.md)
