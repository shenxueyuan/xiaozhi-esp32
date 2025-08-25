# 设置管理系统 (Settings) 详解

## 📁 模块概览
- **功能**: 基于NVS的配置存储和管理系统
- **设计模式**: RAII模式 + 命名空间隔离
- **存储引擎**: ESP32 NVS (Non-Volatile Storage)
- **核心特性**: 类型安全、默认值支持、自动同步

## 🏗️ 架构设计

### NVS存储层次
```
NVS Flash
├── Namespace: "board"           # 硬件相关配置
├── Namespace: "audio"           # 音频系统配置
├── Namespace: "wifi"            # WiFi网络配置
├── Namespace: "mqtt"            # MQTT协议配置
├── Namespace: "websocket"       # WebSocket协议配置
├── Namespace: "display"         # 显示系统配置
└── Namespace: "user"            # 用户自定义配置
```

## 🎯 Settings类核心分析

### 类接口设计
```cpp
class Settings {
public:
    // 构造函数 - 自动打开NVS命名空间
    Settings(const std::string& ns, bool read_write = false);
    ~Settings();  // RAII自动关闭

    // 字符串操作
    std::string GetString(const std::string& key, const std::string& default_value = "");
    void SetString(const std::string& key, const std::string& value);

    // 整数操作
    int32_t GetInt(const std::string& key, int32_t default_value = 0);
    void SetInt(const std::string& key, int32_t value);

    // 布尔操作
    bool GetBool(const std::string& key, bool default_value = false);
    void SetBool(const std::string& key, bool value);

    // 键值管理
    void EraseKey(const std::string& key);
    void EraseAll();

private:
    std::string ns_;                    // 命名空间名称
    nvs_handle_t nvs_handle_ = 0;       // NVS句柄
    bool read_write_ = false;           // 读写模式标志
    bool dirty_ = false;                // 脏数据标志
};
```

### RAII资源管理
```cpp
Settings::Settings(const std::string& ns, bool read_write)
    : ns_(ns), read_write_(read_write), dirty_(false) {

    // 根据模式打开NVS命名空间
    nvs_open_mode_t mode = read_write ? NVS_READWRITE : NVS_READONLY;

    esp_err_t err = nvs_open(ns_.c_str(), mode, &nvs_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s",
                 ns_.c_str(), esp_err_to_name(err));
        nvs_handle_ = 0;
    } else {
        ESP_LOGD(TAG, "Opened NVS namespace: %s (%s)",
                 ns_.c_str(), read_write ? "RW" : "RO");
    }
}

Settings::~Settings() {
    if (nvs_handle_ != 0) {
        // 如果有未提交的更改，自动提交
        if (dirty_ && read_write_) {
            esp_err_t err = nvs_commit(nvs_handle_);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to commit NVS changes: %s",
                         esp_err_to_name(err));
            } else {
                ESP_LOGD(TAG, "NVS changes committed for namespace: %s", ns_.c_str());
            }
        }

        nvs_close(nvs_handle_);
        ESP_LOGD(TAG, "Closed NVS namespace: %s", ns_.c_str());
    }
}
```

### 字符串操作实现
```cpp
std::string Settings::GetString(const std::string& key, const std::string& default_value) {
    if (nvs_handle_ == 0) {
        ESP_LOGW(TAG, "NVS handle is invalid, returning default value");
        return default_value;
    }

    // 首先获取字符串长度
    size_t required_size = 0;
    esp_err_t err = nvs_get_str(nvs_handle_, key.c_str(), nullptr, &required_size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "Key '%s' not found in namespace '%s', using default",
                 key.c_str(), ns_.c_str());
        return default_value;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error getting string size for key '%s': %s",
                 key.c_str(), esp_err_to_name(err));
        return default_value;
    }

    // 分配缓冲区并读取字符串
    std::string value;
    value.resize(required_size - 1);  // 减去null终止符

    err = nvs_get_str(nvs_handle_, key.c_str(), &value[0], &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error getting string value for key '%s': %s",
                 key.c_str(), esp_err_to_name(err));
        return default_value;
    }

    ESP_LOGD(TAG, "Retrieved string: %s='%s'", key.c_str(), value.c_str());
    return value;
}

void Settings::SetString(const std::string& key, const std::string& value) {
    if (nvs_handle_ == 0 || !read_write_) {
        ESP_LOGE(TAG, "Cannot set string: invalid handle or read-only mode");
        return;
    }

    esp_err_t err = nvs_set_str(nvs_handle_, key.c_str(), value.c_str());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting string '%s'='%s': %s",
                 key.c_str(), value.c_str(), esp_err_to_name(err));
    } else {
        dirty_ = true;  // 标记为需要提交
        ESP_LOGD(TAG, "Set string: %s='%s'", key.c_str(), value.c_str());
    }
}
```

### 整数操作实现
```cpp
int32_t Settings::GetInt(const std::string& key, int32_t default_value) {
    if (nvs_handle_ == 0) {
        return default_value;
    }

    int32_t value;
    esp_err_t err = nvs_get_i32(nvs_handle_, key.c_str(), &value);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "Key '%s' not found, using default: %d",
                 key.c_str(), default_value);
        return default_value;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error getting int for key '%s': %s",
                 key.c_str(), esp_err_to_name(err));
        return default_value;
    }

    ESP_LOGD(TAG, "Retrieved int: %s=%d", key.c_str(), value);
    return value;
}

void Settings::SetInt(const std::string& key, int32_t value) {
    if (nvs_handle_ == 0 || !read_write_) {
        ESP_LOGE(TAG, "Cannot set int: invalid handle or read-only mode");
        return;
    }

    esp_err_t err = nvs_set_i32(nvs_handle_, key.c_str(), value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting int '%s'=%d: %s",
                 key.c_str(), value, esp_err_to_name(err));
    } else {
        dirty_ = true;
        ESP_LOGD(TAG, "Set int: %s=%d", key.c_str(), value);
    }
}
```

### 布尔操作实现
```cpp
bool Settings::GetBool(const std::string& key, bool default_value) {
    // 布尔值作为uint8_t存储 (0/1)
    if (nvs_handle_ == 0) {
        return default_value;
    }

    uint8_t value;
    esp_err_t err = nvs_get_u8(nvs_handle_, key.c_str(), &value);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "Key '%s' not found, using default: %s",
                 key.c_str(), default_value ? "true" : "false");
        return default_value;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error getting bool for key '%s': %s",
                 key.c_str(), esp_err_to_name(err));
        return default_value;
    }

    bool result = (value != 0);
    ESP_LOGD(TAG, "Retrieved bool: %s=%s", key.c_str(), result ? "true" : "false");
    return result;
}

void Settings::SetBool(const std::string& key, bool value) {
    if (nvs_handle_ == 0 || !read_write_) {
        ESP_LOGE(TAG, "Cannot set bool: invalid handle or read-only mode");
        return;
    }

    uint8_t uint_value = value ? 1 : 0;
    esp_err_t err = nvs_set_u8(nvs_handle_, key.c_str(), uint_value);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting bool '%s'=%s: %s",
                 key.c_str(), value ? "true" : "false", esp_err_to_name(err));
    } else {
        dirty_ = true;
        ESP_LOGD(TAG, "Set bool: %s=%s", key.c_str(), value ? "true" : "false");
    }
}
```

## 🔧 配置管理模式

### 只读配置查询
```cpp
// 只读模式 - 仅查询配置，不修改
void QueryConfiguration() {
    Settings audio_settings("audio", false);  // 只读模式

    int sample_rate = audio_settings.GetInt("sample_rate", 16000);
    bool enable_aec = audio_settings.GetBool("enable_aec", true);
    std::string codec_type = audio_settings.GetString("codec_type", "es8311");

    ESP_LOGI(TAG, "Audio config: rate=%d, aec=%s, codec=%s",
             sample_rate, enable_aec ? "yes" : "no", codec_type.c_str());
}
```

### 读写配置修改
```cpp
// 读写模式 - 可以修改配置
void UpdateConfiguration() {
    Settings audio_settings("audio", true);   // 读写模式

    // 更新配置
    audio_settings.SetInt("sample_rate", 24000);
    audio_settings.SetBool("enable_aec", true);
    audio_settings.SetString("codec_type", "es8374");

    // 析构时自动提交更改到NVS
}
```

### 配置初始化
```cpp
void InitializeDefaultConfig() {
    Settings board_settings("board", true);

    // 只在首次启动时设置默认值
    if (board_settings.GetString("uuid").empty()) {
        std::string uuid = GenerateUuid();
        board_settings.SetString("uuid", uuid);
        ESP_LOGI(TAG, "Generated new device UUID: %s", uuid.c_str());
    }

    // 设置默认亮度
    if (board_settings.GetInt("backlight_brightness", -1) == -1) {
        board_settings.SetInt("backlight_brightness", 80);
        ESP_LOGI(TAG, "Set default backlight brightness: 80");
    }
}
```

## 🌐 模块配置示例

### WiFi网络配置
```cpp
class WifiConfiguration {
public:
    static void SaveCredentials(const std::string& ssid, const std::string& password) {
        Settings wifi_settings("wifi", true);

        wifi_settings.SetString("ssid", ssid);
        wifi_settings.SetString("password", password);
        wifi_settings.SetBool("configured", true);

        ESP_LOGI(TAG, "WiFi credentials saved for SSID: %s", ssid.c_str());
    }

    static bool LoadCredentials(std::string& ssid, std::string& password) {
        Settings wifi_settings("wifi", false);

        if (!wifi_settings.GetBool("configured", false)) {
            return false;
        }

        ssid = wifi_settings.GetString("ssid");
        password = wifi_settings.GetString("password");

        return !ssid.empty();
    }

    static void ClearCredentials() {
        Settings wifi_settings("wifi", true);

        wifi_settings.EraseKey("ssid");
        wifi_settings.EraseKey("password");
        wifi_settings.SetBool("configured", false);

        ESP_LOGI(TAG, "WiFi credentials cleared");
    }
};
```

### 音频系统配置
```cpp
class AudioConfiguration {
public:
    struct AudioConfig {
        int input_sample_rate = 16000;
        int output_sample_rate = 16000;
        bool enable_aec = true;
        bool enable_vad = true;
        int vad_mode = 3;
        int output_volume = 70;
        std::string codec_type = "es8311";
    };

    static void SaveConfig(const AudioConfig& config) {
        Settings audio_settings("audio", true);

        audio_settings.SetInt("input_sample_rate", config.input_sample_rate);
        audio_settings.SetInt("output_sample_rate", config.output_sample_rate);
        audio_settings.SetBool("enable_aec", config.enable_aec);
        audio_settings.SetBool("enable_vad", config.enable_vad);
        audio_settings.SetInt("vad_mode", config.vad_mode);
        audio_settings.SetInt("output_volume", config.output_volume);
        audio_settings.SetString("codec_type", config.codec_type);
    }

    static AudioConfig LoadConfig() {
        Settings audio_settings("audio", false);

        AudioConfig config;
        config.input_sample_rate = audio_settings.GetInt("input_sample_rate", 16000);
        config.output_sample_rate = audio_settings.GetInt("output_sample_rate", 16000);
        config.enable_aec = audio_settings.GetBool("enable_aec", true);
        config.enable_vad = audio_settings.GetBool("enable_vad", true);
        config.vad_mode = audio_settings.GetInt("vad_mode", 3);
        config.output_volume = audio_settings.GetInt("output_volume", 70);
        config.codec_type = audio_settings.GetString("codec_type", "es8311");

        return config;
    }
};
```

### 协议配置管理
```cpp
class ProtocolConfiguration {
public:
    static void SaveMqttConfig(const std::string& endpoint,
                              const std::string& client_id,
                              const std::string& username,
                              const std::string& password) {
        Settings mqtt_settings("mqtt", true);

        mqtt_settings.SetString("endpoint", endpoint);
        mqtt_settings.SetString("client_id", client_id);
        mqtt_settings.SetString("username", username);
        mqtt_settings.SetString("password", password);
        mqtt_settings.SetInt("keepalive", 240);
    }

    static void SaveWebSocketConfig(const std::string& url,
                                   const std::string& token,
                                   int version = 1) {
        Settings ws_settings("websocket", true);

        ws_settings.SetString("url", url);
        ws_settings.SetString("token", token);
        ws_settings.SetInt("version", version);
    }

    static std::string GetProtocolType() {
        Settings protocol_settings("protocol", false);
        return protocol_settings.GetString("type", "websocket");
    }

    static void SetProtocolType(const std::string& type) {
        Settings protocol_settings("protocol", true);
        protocol_settings.SetString("type", type);
    }
};
```

## 🚀 高级功能

### 配置版本管理
```cpp
class ConfigurationManager {
private:
    static constexpr int CURRENT_CONFIG_VERSION = 2;

public:
    static void MigrateConfig() {
        Settings system_settings("system", true);
        int version = system_settings.GetInt("config_version", 1);

        if (version < CURRENT_CONFIG_VERSION) {
            ESP_LOGI(TAG, "Migrating configuration from v%d to v%d",
                     version, CURRENT_CONFIG_VERSION);

            switch (version) {
                case 1:
                    MigrateFromV1ToV2();
                    // 继续到下一个版本...
                    break;
            }

            system_settings.SetInt("config_version", CURRENT_CONFIG_VERSION);
        }
    }

private:
    static void MigrateFromV1ToV2() {
        // 从v1迁移到v2的逻辑
        Settings audio_settings("audio", true);

        // v2中添加了新的VAD模式配置
        if (audio_settings.GetInt("vad_mode", -1) == -1) {
            audio_settings.SetInt("vad_mode", 3);  // 默认高灵敏度
        }

        ESP_LOGI(TAG, "Audio config migrated to v2");
    }
};
```

### 配置备份和恢复
```cpp
class ConfigurationBackup {
public:
    static void BackupAllSettings() {
        Settings backup_settings("backup", true);

        // 备份时间戳
        backup_settings.SetInt("backup_time", time(nullptr));

        // 备份各个命名空间的关键配置
        BackupNamespace("wifi", backup_settings);
        BackupNamespace("audio", backup_settings);
        BackupNamespace("display", backup_settings);

        ESP_LOGI(TAG, "Configuration backup completed");
    }

    static bool RestoreFromBackup() {
        Settings backup_settings("backup", false);

        int backup_time = backup_settings.GetInt("backup_time", 0);
        if (backup_time == 0) {
            ESP_LOGW(TAG, "No backup found");
            return false;
        }

        ESP_LOGI(TAG, "Restoring configuration from backup (timestamp: %d)", backup_time);

        RestoreNamespace("wifi", backup_settings);
        RestoreNamespace("audio", backup_settings);
        RestoreNamespace("display", backup_settings);

        return true;
    }

private:
    static void BackupNamespace(const std::string& ns, Settings& backup) {
        Settings source(ns, false);

        // 备份关键配置项 (具体实现根据需要)
        // 这里只是示例，实际需要根据各命名空间的配置项来实现
    }

    static void RestoreNamespace(const std::string& ns, Settings& backup) {
        Settings target(ns, true);

        // 从备份恢复配置项
        // 具体实现根据备份的数据结构
    }
};
```

## ⚡ 性能优化

### 延迟提交优化
```cpp
class OptimizedSettings {
private:
    Settings settings_;
    std::map<std::string, std::variant<int32_t, bool, std::string>> pending_changes_;
    std::chrono::steady_clock::time_point last_commit_;

public:
    OptimizedSettings(const std::string& ns) : settings_(ns, true) {}

    void SetInt(const std::string& key, int32_t value) {
        pending_changes_[key] = value;
        ScheduleCommit();
    }

    void SetBool(const std::string& key, bool value) {
        pending_changes_[key] = value;
        ScheduleCommit();
    }

    void SetString(const std::string& key, const std::string& value) {
        pending_changes_[key] = value;
        ScheduleCommit();
    }

    void Flush() {
        CommitPendingChanges();
    }

private:
    void ScheduleCommit() {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_commit_).count() > 1000) {
            CommitPendingChanges();
        }
    }

    void CommitPendingChanges() {
        for (const auto& [key, value] : pending_changes_) {
            std::visit([&](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, int32_t>) {
                    settings_.SetInt(key, v);
                } else if constexpr (std::is_same_v<T, bool>) {
                    settings_.SetBool(key, v);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    settings_.SetString(key, v);
                }
            }, value);
        }

        pending_changes_.clear();
        last_commit_ = std::chrono::steady_clock::now();
    }
};
```

### 缓存机制
```cpp
class CachedSettings {
private:
    Settings settings_;
    mutable std::unordered_map<std::string, std::string> string_cache_;
    mutable std::unordered_map<std::string, int32_t> int_cache_;
    mutable std::unordered_map<std::string, bool> bool_cache_;

public:
    CachedSettings(const std::string& ns) : settings_(ns, false) {}

    std::string GetString(const std::string& key, const std::string& default_value = "") const {
        auto it = string_cache_.find(key);
        if (it != string_cache_.end()) {
            return it->second;
        }

        std::string value = settings_.GetString(key, default_value);
        string_cache_[key] = value;
        return value;
    }

    // 类似地实现 GetInt 和 GetBool 的缓存版本

    void InvalidateCache() {
        string_cache_.clear();
        int_cache_.clear();
        bool_cache_.clear();
    }
};
```

---

**相关文档**:
- [Application主控制器](../02-main-core/02-application-class.md)
- [Board硬件抽象层](../05-board-abstraction/01-board-base.md)
- [通信协议系统](../06-protocols/01-protocol-overview.md)
