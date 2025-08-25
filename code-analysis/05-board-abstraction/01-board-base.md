# 硬件抽象层 (Board) 系统详解

## 📁 模块概览
- **文件数量**: 36个文件 (19个.h + 17个.cc)
- **功能**: 硬件平台抽象，支持70+种不同开发板
- **设计模式**: 单例模式 + 工厂模式 + 模板方法模式
- **核心特色**: 统一接口，插件化支持

## 🏗️ 架构设计

### 继承层次结构
```
Board (抽象基类)
├── WifiBoard                    # WiFi板卡基类
│   ├── EspSparkBot             # ESP-SparkBot
│   ├── EspBox                  # ESP-Box系列
│   ├── AtomS3EchoBase         # M5Stack Atom系列
│   └── ... (50+个WiFi开发板)
├── Ml307Board                  # 4G模块板卡基类
│   ├── XingzhiCube4G          # 星知方块4G版
│   └── ... (4G开发板)
└── DualNetworkBoard            # 双网络板卡
    ├── XingzhiCubeDual        # 支持WiFi+4G切换
    └── ... (双网络开发板)
```

## 🎯 Board基类分析

### 核心接口定义
```cpp
class Board {
private:
    Board(const Board&) = delete;            // 禁用拷贝构造
    Board& operator=(const Board&) = delete; // 禁用赋值操作

protected:
    Board();
    std::string GenerateUuid();              // 生成设备UUID
    std::string uuid_;                       // 软件生成的设备唯一标识

public:
    // 单例模式获取实例
    static Board& GetInstance() {
        static Board* instance = static_cast<Board*>(create_board());
        return *instance;
    }

    virtual ~Board() = default;

    // 基本信息接口
    virtual std::string GetBoardType() = 0;
    virtual std::string GetUuid() { return uuid_; }

    // 硬件组件接口
    virtual Backlight* GetBacklight() { return nullptr; }
    virtual Led* GetLed();
    virtual AudioCodec* GetAudioCodec() = 0;
    virtual Display* GetDisplay();
    virtual Camera* GetCamera();

    // 网络接口
    virtual NetworkInterface* GetNetwork() = 0;
    virtual void StartNetwork() = 0;
    virtual const char* GetNetworkStateIcon() = 0;

    // 系统功能接口
    virtual bool GetTemperature(float& esp32temp);
    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging);
    virtual void SetPowerSaveMode(bool enabled) = 0;

    // 数据接口
    virtual std::string GetJson();
    virtual std::string GetBoardJson() = 0;
    virtual std::string GetDeviceStatusJson() = 0;
};
```

### 工厂模式实现

#### DECLARE_BOARD 宏定义
```cpp
#define DECLARE_BOARD(BOARD_CLASS_NAME) \
void* create_board() { \
    return new BOARD_CLASS_NAME(); \
}
```

#### 工厂函数调用流程
```cpp
// 1. 编译时确定开发板类型
extern void* create_board();

// 2. 运行时创建实例
static Board& GetInstance() {
    static Board* instance = static_cast<Board*>(create_board());
    return *instance;
}

// 3. 具体开发板中使用宏声明
// 例如在 esp_sparkbot_board.cc 中：
DECLARE_BOARD(EspSparkBot);
```

### UUID生成机制
```cpp
std::string Board::GenerateUuid() {
    // UUID v4 需要 16 字节的随机数据
    uint8_t uuid[16];

    // 使用 ESP32 的硬件随机数生成器
    esp_fill_random(uuid, sizeof(uuid));

    // 设置版本 (版本 4) 和变体位
    uuid[6] = (uuid[6] & 0x0F) | 0x40;    // 版本 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80;    // 变体 1

    // 将字节转换为标准的 UUID 字符串格式
    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11],
        uuid[12], uuid[13], uuid[14], uuid[15]);

    return std::string(uuid_str);
}
```

### 默认实现

#### 基础组件默认实现
```cpp
Display* Board::GetDisplay() {
    static NoDisplay display;  // 无显示实现
    return &display;
}

Camera* Board::GetCamera() {
    return nullptr;            // 默认无摄像头
}

Led* Board::GetLed() {
    static NoLed led;          // 无LED实现
    return &led;
}

bool Board::GetBatteryLevel(int &level, bool& charging, bool& discharging) {
    return false;              // 默认无电池
}

bool Board::GetTemperature(float& esp32temp) {
    return false;              // 默认无温度传感器
}
```

## 📶 WifiBoard 基类分析

### WiFi板卡通用功能
```cpp
class WifiBoard : public Board {
protected:
    bool wifi_config_mode_ = false;        // WiFi配置模式标志
    void EnterWifiConfigMode();            // 进入WiFi配置模式
    virtual std::string GetBoardJson() override;

public:
    WifiBoard();
    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual NetworkInterface* GetNetwork() override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveMode(bool enabled) override;
    virtual void ResetWifiConfiguration();
    virtual AudioCodec* GetAudioCodec() override { return nullptr; }
    virtual std::string GetDeviceStatusJson() override;
};
```

### WiFi配置模式实现
```cpp
void WifiBoard::EnterWifiConfigMode() {
    ESP_LOGI(TAG, "Entering WiFi configuration mode");
    wifi_config_mode_ = true;

    // 启动AP模式进行配置
    WifiStation::GetInstance().StartConfigPortal();

    // 显示配置提示
    auto display = GetDisplay();
    if (display) {
        display->ShowWifiConfigMode();
    }
}

void WifiBoard::ResetWifiConfiguration() {
    ESP_LOGI(TAG, "Resetting WiFi configuration");

    // 清除保存的WiFi配置
    WifiStation::GetInstance().ClearConfiguration();

    // 重启设备应用配置
    esp_restart();
}
```

### 网络状态管理
```cpp
const char* WifiBoard::GetNetworkStateIcon() {
    auto& wifi = WifiStation::GetInstance();

    if (wifi_config_mode_) {
        return ICON_WIFI_CONFIG;      // 配置模式图标
    } else if (wifi.IsConnected()) {
        int rssi = wifi.GetRSSI();
        if (rssi > -50) {
            return ICON_WIFI_STRONG;   // 强信号
        } else if (rssi > -70) {
            return ICON_WIFI_MEDIUM;   // 中等信号
        } else {
            return ICON_WIFI_WEAK;     // 弱信号
        }
    } else {
        return ICON_WIFI_DISCONNECTED; // 未连接
    }
}
```

## 📡 Ml307Board 基类分析

### 4G模块板卡功能
```cpp
class Ml307Board : public Board {
private:
    gpio_num_t tx_pin_;                    // UART TX引脚
    gpio_num_t rx_pin_;                    // UART RX引脚
    gpio_num_t dtr_pin_;                   // DTR控制引脚

public:
    Ml307Board(gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t dtr_pin = GPIO_NUM_NC);

    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual NetworkInterface* GetNetwork() override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveMode(bool enabled) override;
    virtual std::string GetBoardJson() override;
    virtual std::string GetDeviceStatusJson() override;
};
```

### ML307模块初始化
```cpp
Ml307Board::Ml307Board(gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t dtr_pin)
    : Board(), tx_pin_(tx_pin), rx_pin_(rx_pin), dtr_pin_(dtr_pin) {

    // 初始化UART通信
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, 1024, 1024, 0, NULL, 0);

    // 初始化DTR控制引脚
    if (dtr_pin_ != GPIO_NUM_NC) {
        gpio_config_t dtr_config = {
            .pin_bit_mask = (1ULL << dtr_pin_),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&dtr_config);
        gpio_set_level(dtr_pin_, 0);
    }
}
```

## 🔄 DualNetworkBoard 分析

### 双网络切换机制
```cpp
class DualNetworkBoard : public Board {
private:
    std::unique_ptr<Board> current_board_;     // 当前活动的板卡
    NetworkType network_type_ = NetworkType::ML307;  // 当前网络类型

    // ML307配置
    gpio_num_t ml307_tx_pin_;
    gpio_num_t ml307_rx_pin_;
    gpio_num_t ml307_dtr_pin_;

    // 网络类型管理
    NetworkType LoadNetworkTypeFromSettings(int32_t default_net_type);
    void SaveNetworkTypeToSettings(NetworkType type);
    void InitializeCurrentBoard();

public:
    DualNetworkBoard(gpio_num_t ml307_tx_pin, gpio_num_t ml307_rx_pin,
                     gpio_num_t ml307_dtr_pin = GPIO_NUM_NC,
                     int32_t default_net_type = 1);

    // 网络切换
    void SwitchNetworkType();
    NetworkType GetNetworkType() const { return network_type_; }
    Board& GetCurrentBoard() const { return *current_board_; }

    // 重写Board接口，委托给当前活动板卡
    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual NetworkInterface* GetNetwork() override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveMode(bool enabled) override;
    virtual std::string GetBoardJson() override;
    virtual std::string GetDeviceStatusJson() override;
};
```

### 网络类型切换实现
```cpp
void DualNetworkBoard::SwitchNetworkType() {
    auto display = GetDisplay();
    if (display) {
        display->ShowMessage("切换网络中...");
    }

    // 切换网络类型
    network_type_ = (network_type_ == NetworkType::ML307)
                    ? NetworkType::WIFI
                    : NetworkType::ML307;

    // 保存到设置
    SaveNetworkTypeToSettings(network_type_);

    // 重新初始化当前板卡
    InitializeCurrentBoard();

    // 启动新网络
    StartNetwork();

    ESP_LOGI(TAG, "Network switched to: %s",
             (network_type_ == NetworkType::ML307) ? "ML307" : "WiFi");
}

void DualNetworkBoard::InitializeCurrentBoard() {
    if (network_type_ == NetworkType::ML307) {
        ESP_LOGI(TAG, "Initialize ML307 board");
        current_board_ = std::make_unique<Ml307Board>(ml307_tx_pin_, ml307_rx_pin_, ml307_dtr_pin_);
    } else {
        ESP_LOGI(TAG, "Initialize WiFi board");
        current_board_ = std::make_unique<WifiBoard>();
    }
}
```

## 🔧 通用组件抽象

### Button 按钮处理
```cpp
class Button {
private:
    gpio_num_t pin_;
    bool pressed_state_;
    std::function<void()> click_callback_;
    std::function<void()> long_press_callback_;

    uint32_t last_press_time_;
    uint32_t long_press_duration_;
    bool is_pressed_;

public:
    Button(gpio_num_t pin, bool pressed_state = false, uint32_t long_press_ms = 3000);

    void OnClick(std::function<void()> callback);
    void OnLongPress(std::function<void()> callback);
    void SetLongPressDuration(uint32_t ms);

private:
    static void ButtonTask(void* arg);
    void HandleButtonEvent();
};
```

### I2cDevice I2C设备抽象
```cpp
class I2cDevice {
protected:
    i2c_master_bus_handle_t bus_handle_;
    i2c_master_dev_handle_t dev_handle_;
    uint8_t device_addr_;

public:
    I2cDevice(i2c_master_bus_handle_t bus_handle, uint8_t device_addr);
    virtual ~I2cDevice();

    bool WriteRegister(uint8_t reg, uint8_t value);
    bool ReadRegister(uint8_t reg, uint8_t& value);
    bool WriteRegisters(uint8_t reg, const uint8_t* data, size_t length);
    bool ReadRegisters(uint8_t reg, uint8_t* data, size_t length);

protected:
    virtual bool Initialize() = 0;
};
```

### Camera 摄像头抽象
```cpp
class Camera {
public:
    virtual ~Camera() = default;

    virtual bool Initialize() = 0;
    virtual bool StartStream() = 0;
    virtual void StopStream() = 0;
    virtual bool CaptureFrame(std::vector<uint8_t>& frame_data) = 0;
    virtual void SetResolution(camera_resolution_t resolution) = 0;
    virtual void SetPixelFormat(camera_pixel_format_t format) = 0;
    virtual bool IsStreaming() const = 0;
};
```

### Backlight 背光控制
```cpp
class Backlight {
public:
    virtual ~Backlight() = default;

    virtual void SetBrightness(int brightness) = 0;  // 0-100
    virtual int GetBrightness() const = 0;
    virtual void TurnOn() = 0;
    virtual void TurnOff() = 0;
    virtual void SaveBrightness() = 0;
    virtual void RestoreBrightness() = 0;
};

class PwmBacklight : public Backlight {
private:
    gpio_num_t pin_;
    bool output_invert_;
    int current_brightness_;
    ledc_channel_t ledc_channel_;

public:
    PwmBacklight(gpio_num_t pin, bool output_invert = false);

    void SetBrightness(int brightness) override;
    int GetBrightness() const override;
    void TurnOn() override;
    void TurnOff() override;
    void SaveBrightness() override;
    void RestoreBrightness() override;
};
```

## ⚡ 性能优化特性

### 单例模式优化
```cpp
// 线程安全的单例实现
static Board& GetInstance() {
    static Board* instance = static_cast<Board*>(create_board());
    return *instance;
}
// C++11保证static局部变量的线程安全初始化
```

### 内存管理优化
```cpp
// 智能指针管理
std::unique_ptr<Board> current_board_;

// 静态对象避免动态分配
static NoDisplay display;
static NoLed led;
```

### 延迟初始化
```cpp
// 组件按需初始化
virtual AudioCodec* GetAudioCodec() override {
    static Es8311AudioCodec audio_codec(...);  // 第一次调用时初始化
    return &audio_codec;
}
```

## 🔗 与其他模块的集成

### Application集成
```cpp
void Application::Start() {
    // 获取板卡实例
    auto& board = Board::GetInstance();

    // 初始化各组件
    audio_codec_ = board.GetAudioCodec();
    display_ = board.GetDisplay();
    camera_ = board.GetCamera();

    // 启动网络
    board.StartNetwork();

    ESP_LOGI(TAG, "Board type: %s", board.GetBoardType().c_str());
    ESP_LOGI(TAG, "Device UUID: %s", board.GetUuid().c_str());
}
```

### 配置系统集成
```cpp
// 板卡特定配置
Settings board_settings(board.GetBoardType(), true);
int brightness = board_settings.GetInt("backlight_brightness", 80);
bool camera_enabled = board_settings.GetBool("camera_enabled", true);

// 应用配置
board.GetBacklight()->SetBrightness(brightness);
if (camera_enabled) {
    board.GetCamera()->Initialize();
}
```

## 🎯 开发板实现示例

### ESP-SparkBot 实现概览
```cpp
class EspSparkBot : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Display* display_;
    Esp32Camera* camera_;

    void InitializeI2c();
    void InitializeSpi();
    void InitializeDisplay();
    void InitializeButtons();
    void InitializeCamera();

public:
    EspSparkBot() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeCamera();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override;
    virtual Display* GetDisplay() override;
    virtual Backlight* GetBacklight() override;
    virtual Camera* GetCamera() override;
};

// 注册为工厂类
DECLARE_BOARD(EspSparkBot);
```

---

**相关文档**:
- [具体开发板实现分析](./02-board-implementations.md)
- [WiFi网络实现](../06-protocols/03-network-interfaces.md)
- [Application核心分析](../02-main-core/02-application-class.md)
