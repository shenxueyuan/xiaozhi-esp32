# OTA升级系统 (OTA) 详解

## 📁 模块概览
- **功能**: 无线固件升级系统
- **核心特性**: 版本检查、安全下载、分区管理、回滚机制
- **升级策略**: 双分区切换 + 版本验证 + 进度反馈
- **安全机制**: 数字签名验证、镜像头部检查、CRC校验

## 🏗️ OTA架构设计

### 分区布局
```
ESP32 Flash Layout (16MB)
├── bootloader (64KB)          # 引导加载器
├── partition_table (4KB)      # 分区表
├── nvs (64KB)                 # 配置存储
├── phy_init (4KB)             # RF初始化数据
├── factory (6MB)              # 出厂固件 (可选)
├── ota_0 (6MB)                # OTA分区0 (主分区)
├── ota_1 (6MB)                # OTA分区1 (备份分区)
└── spiffs (1MB)               # 文件系统
```

### 升级状态机
```
[当前运行] → [检查版本] → [下载固件] → [验证镜像] → [切换分区] → [重启验证]
     ↓            ↓            ↓            ↓            ↓            ↓
  运行中      → 有新版本    → 下载中     → 验证成功   → 设置启动   → 标记有效
     ↑                                     ↓                       ↓
     └─────── [回滚] ←─── [验证失败] ←──── [镜像损坏] ←─────────────┘
```

## 🎯 Ota类核心分析

### 类接口设计
```cpp
class Ota {
public:
    Ota();
    ~Ota();

    // 版本管理
    bool CheckVersion();                                    // 检查是否有新版本
    bool HasNewVersion() const { return has_new_version_; } // 是否有新版本可用
    std::string GetFirmwareVersion() const { return firmware_version_; }

    // 升级控制
    bool StartUpgrade(std::function<void(int, size_t)> progress_callback = nullptr);
    void MarkCurrentVersionValid();                        // 标记当前版本为有效

    // 设备认证
    bool HasSerialNumber() const { return has_serial_number_; }

private:
    // 网络操作
    std::string GetCheckVersionUrl();
    std::unique_ptr<Http> SetupHttp();
    bool Upgrade(const std::string& firmware_url);

    // 版本比较
    bool IsNewVersionAvailable(const std::string& current, const std::string& new_version);

    // 成员变量
    bool has_new_version_ = false;                         // 是否有新版本
    std::string firmware_version_;                         // 新固件版本号
    std::string firmware_url_;                             // 新固件下载URL
    bool has_serial_number_ = false;                       // 是否有序列号
    std::string serial_number_;                            // 设备序列号
};
```

### 设备认证机制
```cpp
Ota::Ota() {
#ifdef ESP_EFUSE_BLOCK_USR_DATA
    // 从eFuse用户数据区读取序列号
    uint8_t serial_number[33] = {0};
    if (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, serial_number, 32 * 8) == ESP_OK) {
        if (serial_number[0] == 0) {
            has_serial_number_ = false;
            ESP_LOGI(TAG, "No serial number found in eFuse");
        } else {
            // 序列号存在，提取32字节数据
            serial_number_ = std::string(reinterpret_cast<char*>(serial_number), 32);
            has_serial_number_ = true;
            ESP_LOGI(TAG, "Serial number loaded from eFuse: %s",
                     serial_number_.substr(0, 8).c_str()); // 只显示前8位
        }
    } else {
        ESP_LOGE(TAG, "Failed to read serial number from eFuse");
        has_serial_number_ = false;
    }
#else
    ESP_LOGW(TAG, "eFuse user data not supported on this chip");
    has_serial_number_ = false;
#endif
}
```

### HTTP客户端配置
```cpp
std::unique_ptr<Http> Ota::SetupHttp() {
    auto& board = Board::GetInstance();
    auto app_desc = esp_app_get_description();

    auto network = board.GetNetwork();
    auto http = network->CreateHttp(0);  // 创建HTTP客户端

    // 设备认证头部
    http->SetHeader("Activation-Version", has_serial_number_ ? "2" : "1");
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", board.GetUuid());

    // 序列号认证 (如果可用)
    if (has_serial_number_) {
        http->SetHeader("Serial-Number", serial_number_.c_str());
    }

    // 设备信息
    http->SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);
    http->SetHeader("Accept-Language", Lang::CODE);  // 支持多语言
    http->SetHeader("Content-Type", "application/json");

    return http;
}
```

## 🔍 版本检查流程

### CheckVersion实现详解
```cpp
bool Ota::CheckVersion() {
    ESP_LOGI(TAG, "Checking for firmware updates...");

    auto http = SetupHttp();
    std::string check_url = GetCheckVersionUrl();

    if (!http->Open("GET", check_url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection to: %s", check_url.c_str());
        return false;
    }

    // 检查HTTP响应状态
    int status_code = http->GetStatusCode();
    if (status_code != 200) {
        ESP_LOGE(TAG, "Server returned status: %d", status_code);
        http->Close();
        return false;
    }

    // 读取响应体
    std::string response_body;
    char buffer[512];
    int bytes_read;

    while ((bytes_read = http->Read(buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        response_body += buffer;
    }

    http->Close();

    // 解析JSON响应
    return ParseVersionResponse(response_body);
}
```

### 版本信息解析
```cpp
bool Ota::ParseVersionResponse(const std::string& json_response) {
    cJSON* root = cJSON_Parse(json_response.c_str());
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return false;
    }

    // 获取当前版本信息
    auto app_desc = esp_app_get_description();
    std::string current_version = app_desc->version;

    has_new_version_ = false;

    // 解析固件信息
    cJSON* firmware = cJSON_GetObjectItem(root, "firmware");
    if (cJSON_IsObject(firmware)) {
        cJSON* version = cJSON_GetObjectItem(firmware, "version");
        cJSON* url = cJSON_GetObjectItem(firmware, "url");

        if (cJSON_IsString(version) && cJSON_IsString(url)) {
            firmware_version_ = version->valuestring;
            firmware_url_ = url->valuestring;

            // 版本比较
            has_new_version_ = IsNewVersionAvailable(current_version, firmware_version_);

            if (has_new_version_) {
                ESP_LOGI(TAG, "New version available: %s → %s",
                         current_version.c_str(), firmware_version_.c_str());
            } else {
                ESP_LOGI(TAG, "Current version %s is up to date", current_version.c_str());
            }

            // 强制升级标志
            cJSON* force = cJSON_GetObjectItem(firmware, "force");
            if (cJSON_IsNumber(force) && force->valueint == 1) {
                ESP_LOGW(TAG, "Force upgrade flag detected");
                has_new_version_ = true;
            }
        }
    } else {
        ESP_LOGW(TAG, "No firmware section found in response");
    }

    cJSON_Delete(root);
    return true;
}
```

### 语义版本比较
```cpp
bool Ota::IsNewVersionAvailable(const std::string& current, const std::string& new_version) {
    // 解析版本号 (例如: "1.2.3" → {1, 2, 3})
    auto parse_version = [](const std::string& version) -> std::vector<int> {
        std::vector<int> parts;
        std::stringstream ss(version);
        std::string part;

        while (std::getline(ss, part, '.')) {
            try {
                parts.push_back(std::stoi(part));
            } catch (const std::exception&) {
                parts.push_back(0);  // 无效部分默认为0
            }
        }

        // 确保至少有3个部分 (major.minor.patch)
        while (parts.size() < 3) {
            parts.push_back(0);
        }

        return parts;
    };

    auto current_parts = parse_version(current);
    auto new_parts = parse_version(new_version);

    // 逐个比较版本号组件
    for (size_t i = 0; i < std::max(current_parts.size(), new_parts.size()); i++) {
        int current_part = (i < current_parts.size()) ? current_parts[i] : 0;
        int new_part = (i < new_parts.size()) ? new_parts[i] : 0;

        if (new_part > current_part) {
            return true;   // 新版本更高
        } else if (new_part < current_part) {
            return false;  // 新版本更低
        }
        // 相等则继续比较下一位
    }

    return false;  // 版本完全相同
}
```

## 📦 固件下载和安装

### StartUpgrade主流程
```cpp
bool Ota::StartUpgrade(std::function<void(int, size_t)> progress_callback) {
    if (!has_new_version_) {
        ESP_LOGW(TAG, "No new version available");
        return false;
    }

    ESP_LOGI(TAG, "Starting firmware upgrade to version: %s", firmware_version_.c_str());

    // 执行升级
    bool success = Upgrade(firmware_url_, progress_callback);

    if (success) {
        ESP_LOGI(TAG, "Firmware upgrade completed successfully");
        ESP_LOGI(TAG, "Device will reboot to apply new firmware...");

        // 短暂延迟后重启
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }

    return success;
}
```

### Upgrade实现详解
```cpp
bool Ota::Upgrade(const std::string& firmware_url,
                  std::function<void(int, size_t)> progress_callback) {
    ESP_LOGI(TAG, "Downloading firmware from: %s", firmware_url.c_str());

    // 获取下一个可用的OTA分区
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(nullptr);

    if (update_partition == nullptr) {
        ESP_LOGE(TAG, "No available OTA partition found");
        return false;
    }

    ESP_LOGI(TAG, "Writing to partition: %s (offset: 0x%lx, size: %lu)",
             update_partition->label, update_partition->address, update_partition->size);

    // 建立HTTP连接
    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);

    if (!http->Open("GET", firmware_url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return false;
    }

    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "HTTP request failed, status: %d", http->GetStatusCode());
        return false;
    }

    // 获取文件大小
    size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        ESP_LOGE(TAG, "Invalid content length");
        return false;
    }

    ESP_LOGI(TAG, "Firmware size: %zu bytes", content_length);

    return DownloadAndFlash(http.get(), update_partition, &update_handle,
                           content_length, progress_callback);
}
```

### 下载和烧写过程
```cpp
bool Ota::DownloadAndFlash(Http* http,
                          const esp_partition_t* partition,
                          esp_ota_handle_t* update_handle,
                          size_t total_size,
                          std::function<void(int, size_t)> progress_callback) {

    char buffer[1024];  // 1KB缓冲区
    size_t total_read = 0;
    size_t recent_read = 0;
    bool image_header_checked = false;
    std::string image_header_buffer;

    auto last_progress_time = esp_timer_get_time();
    auto last_calc_time = last_progress_time;

    while (total_read < total_size) {
        int bytes_read = http->Read(buffer, sizeof(buffer));

        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                ESP_LOGI(TAG, "Download completed");
                break;
            } else {
                ESP_LOGE(TAG, "HTTP read error: %d", bytes_read);
                return false;
            }
        }

        // 验证镜像头部 (仅第一次)
        if (!image_header_checked) {
            if (!ValidateImageHeader(buffer, bytes_read, &image_header_buffer,
                                   partition, update_handle)) {
                return false;
            }
            image_header_checked = true;
        }

        // 写入OTA分区
        esp_err_t err = esp_ota_write(*update_handle, buffer, bytes_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
            esp_ota_abort(*update_handle);
            return false;
        }

        total_read += bytes_read;
        recent_read += bytes_read;

        // 更新进度 (每500ms)
        auto current_time = esp_timer_get_time();
        if (current_time - last_progress_time > 500000 && progress_callback) {
            auto time_diff = current_time - last_calc_time;
            size_t speed = (time_diff > 0) ? (recent_read * 1000000 / time_diff) : 0;
            int progress = (total_read * 100) / total_size;

            progress_callback(progress, speed);

            last_progress_time = current_time;
            last_calc_time = current_time;
            recent_read = 0;
        }
    }

    return FinalizeUpgrade(update_handle, partition);
}
```

### 镜像头部验证
```cpp
bool Ota::ValidateImageHeader(const char* buffer, int size,
                             std::string* header_buffer,
                             const esp_partition_t* partition,
                             esp_ota_handle_t* update_handle) {
    // 累积镜像头部数据
    header_buffer->append(buffer, size);

    // 检查是否有足够的数据来解析头部
    size_t required_size = sizeof(esp_image_header_t) +
                          sizeof(esp_image_segment_header_t) +
                          sizeof(esp_app_desc_t);

    if (header_buffer->size() < required_size) {
        return true;  // 需要更多数据
    }

    // 解析应用描述符
    esp_app_desc_t new_app_info;
    size_t offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    memcpy(&new_app_info, header_buffer->data() + offset, sizeof(esp_app_desc_t));

    ESP_LOGI(TAG, "New firmware info:");
    ESP_LOGI(TAG, "  Version: %s", new_app_info.version);
    ESP_LOGI(TAG, "  Project: %s", new_app_info.project_name);
    ESP_LOGI(TAG, "  Compile time: %s %s", new_app_info.date, new_app_info.time);
    ESP_LOGI(TAG, "  IDF version: %s", new_app_info.idf_ver);

    // 版本检查 (防止降级)
    auto current_app_desc = esp_app_get_description();
    if (strcmp(new_app_info.version, current_app_desc->version) == 0) {
        ESP_LOGE(TAG, "Same version detected, aborting upgrade");
        return false;
    }

    // 开始OTA过程
    esp_err_t err = esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "OTA process started successfully");

    // 清理头部缓冲区以节省内存
    header_buffer->clear();
    header_buffer->shrink_to_fit();

    return true;
}
```

### 升级完成和分区切换
```cpp
bool Ota::FinalizeUpgrade(esp_ota_handle_t* update_handle,
                         const esp_partition_t* partition) {
    // 结束OTA写入
    esp_err_t err = esp_ota_end(*update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed - firmware corrupted");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        }
        return false;
    }

    // 设置新的启动分区
    err = esp_ota_set_boot_partition(partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Firmware upgrade completed successfully");
    ESP_LOGI(TAG, "Next boot will use partition: %s", partition->label);

    return true;
}
```

## 🔄 回滚和验证机制

### 启动时版本验证
```cpp
void Ota::MarkCurrentVersionValid() {
    // 获取当前运行的分区
    const esp_partition_t* partition = esp_ota_get_running_partition();

    if (strcmp(partition->label, "factory") == 0) {
        ESP_LOGI(TAG, "Running from factory partition, no validation needed");
        return;
    }

    ESP_LOGI(TAG, "Running from partition: %s", partition->label);

    // 检查分区状态
    esp_ota_img_states_t state;
    esp_err_t err = esp_ota_get_state_partition(partition, &state);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get partition state: %s", esp_err_to_name(err));
        return;
    }

    switch (state) {
        case ESP_OTA_IMG_VALID:
            ESP_LOGI(TAG, "Current firmware is already validated");
            break;

        case ESP_OTA_IMG_PENDING_VERIFY:
            ESP_LOGI(TAG, "Firmware pending verification - marking as valid");
            esp_ota_mark_app_valid_cancel_rollback();
            ESP_LOGI(TAG, "Firmware marked as valid, rollback cancelled");
            break;

        case ESP_OTA_IMG_INVALID:
            ESP_LOGW(TAG, "Current firmware is marked as invalid");
            break;

        case ESP_OTA_IMG_ABORTED:
            ESP_LOGW(TAG, "Current firmware was aborted");
            break;

        default:
            ESP_LOGW(TAG, "Unknown firmware state: %d", state);
            break;
    }
}
```

### 自动回滚机制
```cpp
// 在Application::Start()中调用
void Application::ValidateOtaUpgrade() {
    auto& ota = Ota::GetInstance();

    // 检查是否是新启动的固件
    const esp_partition_t* partition = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(partition, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "New firmware detected, starting validation timer");

            // 启动验证定时器 (例如: 5分钟)
            CreateValidationTimer();
        }
    }
}

void Application::CreateValidationTimer() {
    const TickType_t validation_timeout = pdMS_TO_TICKS(5 * 60 * 1000); // 5分钟

    validation_timer_ = xTimerCreate(
        "ota_validation",
        validation_timeout,
        pdFALSE,  // 单次触发
        this,
        [](TimerHandle_t timer) {
            auto* app = static_cast<Application*>(pvTimerGetTimerID(timer));
            app->OnValidationTimeout();
        }
    );

    if (validation_timer_) {
        xTimerStart(validation_timer_, 0);
        ESP_LOGI(TAG, "OTA validation timer started (5 minutes)");
    }
}

void Application::OnValidationTimeout() {
    ESP_LOGW(TAG, "OTA validation timeout - firmware may be unstable");
    // 可以选择回滚或继续运行

    // 如果系统运行正常，标记为有效
    if (IsSystemHealthy()) {
        Ota::GetInstance().MarkCurrentVersionValid();
        ESP_LOGI(TAG, "System is healthy, firmware validated");
    } else {
        ESP_LOGE(TAG, "System is unhealthy, initiating rollback");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}
```

## ⚡ 性能优化和错误处理

### 下载优化
```cpp
// 自适应缓冲区大小
class AdaptiveBuffer {
private:
    size_t current_size_ = 1024;        // 初始1KB
    size_t max_size_ = 8192;            // 最大8KB
    size_t min_size_ = 512;             // 最小512B
    std::chrono::steady_clock::time_point last_adjust_;

public:
    size_t GetBufferSize(size_t current_speed) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_adjust_);

        if (elapsed.count() >= 5) {  // 每5秒调整一次
            if (current_speed > 100 * 1024) {  // >100KB/s，增大缓冲区
                current_size_ = std::min(current_size_ * 2, max_size_);
            } else if (current_speed < 10 * 1024) {  // <10KB/s，减小缓冲区
                current_size_ = std::max(current_size_ / 2, min_size_);
            }
            last_adjust_ = now;
        }

        return current_size_;
    }
};
```

### 网络错误处理
```cpp
bool Ota::DownloadWithRetry(const std::string& url, int max_retries = 3) {
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        ESP_LOGI(TAG, "Download attempt %d/%d", attempt, max_retries);

        if (Upgrade(url)) {
            return true;  // 成功
        }

        if (attempt < max_retries) {
            // 指数退避重试
            int delay_ms = 1000 * (1 << (attempt - 1));  // 1s, 2s, 4s
            ESP_LOGW(TAG, "Download failed, retrying in %d ms", delay_ms);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    ESP_LOGE(TAG, "All download attempts failed");
    return false;
}
```

### 内存管理优化
```cpp
// 流式处理，避免大内存分配
class StreamingOtaWriter {
private:
    esp_ota_handle_t update_handle_;
    size_t total_written_ = 0;

public:
    bool WriteChunk(const void* data, size_t size) {
        esp_err_t err = esp_ota_write(update_handle_, data, size);
        if (err == ESP_OK) {
            total_written_ += size;
            return true;
        } else {
            ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
            return false;
        }
    }

    void GetProgress(size_t total_size, int& percentage, size_t& written) {
        percentage = (total_written_ * 100) / total_size;
        written = total_written_;
    }
};
```

## 🔗 与其他模块集成

### Application集成
```cpp
void Application::CheckAndHandleOta() {
    static bool ota_checked = false;

    if (!ota_checked && IsNetworkConnected()) {
        ESP_LOGI(TAG, "Checking for firmware updates...");

        auto& ota = Ota::GetInstance();
        if (ota.CheckVersion() && ota.HasNewVersion()) {
            // 显示升级提示
            Alert(Lang::Strings::OTA_UPGRADE,
                  Lang::Strings::UPGRADING,
                  "happy",
                  Lang::Sounds::OGG_UPGRADE);

            // 开始升级流程
            StartOtaUpgrade(ota);
        }

        ota_checked = true;
    }
}

void Application::StartOtaUpgrade(Ota& ota) {
    SetDeviceState(kDeviceStateUpgrading);

    // 停止音频服务节省资源
    audio_service_.Stop();
    Board::GetInstance().SetPowerSaveMode(false);

    // 显示升级界面
    auto display = Board::GetInstance().GetDisplay();
    display->SetIcon(FONT_AWESOME_DOWNLOAD);
    display->SetChatMessage("system", "正在升级固件...");

    // 开始升级 (带进度回调)
    bool success = ota.StartUpgrade([display](int progress, size_t speed) {
        std::string message = "升级中 " + std::to_string(progress) + "% " +
                             std::to_string(speed / 1024) + "KB/s";
        display->SetChatMessage("system", message);
    });

    if (success) {
        display->SetChatMessage("system", "升级完成，即将重启...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        // 升级失败，恢复正常运行
        display->SetChatMessage("system", "升级失败，继续正常运行");
        audio_service_.Start();
        Board::GetInstance().SetPowerSaveMode(true);
    }
}
```

### 配置管理集成
```cpp
class OtaConfiguration {
public:
    static void SaveOtaSettings(const std::string& url, bool auto_check) {
        Settings ota_settings("ota", true);

        ota_settings.SetString("server_url", url);
        ota_settings.SetBool("auto_check", auto_check);
        ota_settings.SetInt("check_interval", 24 * 3600);  // 24小时
    }

    static std::string GetOtaServerUrl() {
        Settings ota_settings("ota", false);
        return ota_settings.GetString("server_url", CONFIG_OTA_URL);
    }

    static bool ShouldAutoCheck() {
        Settings ota_settings("ota", false);
        return ota_settings.GetBool("auto_check", true);
    }
};
```

---

**相关文档**:
- [Application主控制器](../02-main-core/02-application-class.md)
- [设置管理系统](./04-settings-system.md)
- [系统信息模块](./06-system-info.md)
- [网络连接流程](../08-sequence-diagrams/04-network-connection-flow.md)
