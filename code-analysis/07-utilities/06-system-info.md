# 系统信息模块 (SystemInfo) 详解

## 📁 模块概览
- **功能**: 系统硬件信息收集和性能监控
- **设计模式**: 静态工具类 + 单例访问
- **核心特性**: 内存监控、任务分析、芯片信息、性能统计
- **应用场景**: 调试诊断、性能优化、设备管理

## 🏗️ 架构设计

### 信息收集层次
```
SystemInfo (静态工具类)
├── 硬件信息
│   ├── Flash大小和型号
│   ├── 芯片型号和MAC地址
│   └── 内存容量和使用情况
├── 运行时信息
│   ├── 堆内存统计
│   ├── 任务列表和状态
│   └── CPU使用率分析
└── 性能监控
    ├── 最小可用内存
    ├── 任务切换统计
    └── 系统运行时间
```

## 🎯 SystemInfo类核心分析

### 静态接口设计
```cpp
class SystemInfo {
public:
    // 硬件信息获取
    static size_t GetFlashSize();                        // 获取Flash大小
    static std::string GetMacAddress();                  // 获取MAC地址
    static std::string GetChipModelName();               // 获取芯片型号

    // 内存监控
    static size_t GetFreeHeapSize();                     // 当前可用堆内存
    static size_t GetMinimumFreeHeapSize();              // 最小可用堆内存
    static void PrintHeapStats();                       // 打印内存统计

    // 任务和性能分析
    static esp_err_t PrintTaskCpuUsage(TickType_t xTicksToWait);  // CPU使用率分析
    static void PrintTaskList();                        // 任务列表

private:
    // 禁用实例化
    SystemInfo() = delete;
    ~SystemInfo() = delete;
    SystemInfo(const SystemInfo&) = delete;
    SystemInfo& operator=(const SystemInfo&) = delete;
};
```

### 硬件信息收集

#### Flash存储信息
```cpp
size_t SystemInfo::GetFlashSize() {
    uint32_t flash_size;

    // 获取Flash大小
    esp_err_t err = esp_flash_get_size(NULL, &flash_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash size: %s", esp_err_to_name(err));
        return 0;
    }

    ESP_LOGD(TAG, "Flash size: %lu bytes (%.1f MB)",
             flash_size, flash_size / (1024.0 * 1024.0));

    return (size_t)flash_size;
}

// 扩展: Flash详细信息
static void GetFlashInfo() {
    const esp_flash_t* flash = esp_flash_default_chip;
    uint32_t flash_id;

    if (esp_flash_read_id(flash, &flash_id) == ESP_OK) {
        uint8_t manufacturer = (flash_id >> 16) & 0xFF;
        uint8_t memory_type = (flash_id >> 8) & 0xFF;
        uint8_t capacity = flash_id & 0xFF;

        ESP_LOGI(TAG, "Flash ID: 0x%06lX", flash_id);
        ESP_LOGI(TAG, "Manufacturer: 0x%02X", manufacturer);
        ESP_LOGI(TAG, "Memory Type: 0x%02X", memory_type);
        ESP_LOGI(TAG, "Capacity: 0x%02X", capacity);

        // 常见厂商识别
        const char* manufacturer_name = "Unknown";
        switch (manufacturer) {
            case 0x20: manufacturer_name = "Micron"; break;
            case 0xC8: manufacturer_name = "GigaDevice"; break;
            case 0xEF: manufacturer_name = "Winbond"; break;
            case 0x1C: manufacturer_name = "EON"; break;
        }
        ESP_LOGI(TAG, "Flash Manufacturer: %s", manufacturer_name);
    }
}
```

#### 网络接口信息
```cpp
std::string SystemInfo::GetMacAddress() {
    uint8_t mac[6];
    esp_err_t err;

#if CONFIG_IDF_TARGET_ESP32P4
    // ESP32-P4 特殊处理 (通过WiFi远程获取)
    err = esp_wifi_get_mac(WIFI_IF_STA, mac);
#else
    // 标准芯片直接读取
    err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
#endif

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get MAC address: %s", esp_err_to_name(err));
        return "00:00:00:00:00:00";
    }

    // 格式化MAC地址
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return std::string(mac_str);
}

// 扩展: 多种MAC地址获取
static void GetAllMacAddresses() {
    uint8_t mac[6];

    // WiFi STA MAC
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi STA MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    // WiFi AP MAC
    if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi AP MAC:  %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    // Bluetooth MAC
    if (esp_read_mac(mac, ESP_MAC_BT) == ESP_OK) {
        ESP_LOGI(TAG, "Bluetooth MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    // Ethernet MAC
    if (esp_read_mac(mac, ESP_MAC_ETH) == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}
```

#### 芯片信息识别
```cpp
std::string SystemInfo::GetChipModelName() {
    // 直接返回编译时配置的目标芯片
    return std::string(CONFIG_IDF_TARGET);
}

// 扩展: 详细芯片信息
static void GetDetailedChipInfo() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    const char* chip_model = "Unknown";
    switch (chip_info.model) {
        case CHIP_ESP32:   chip_model = "ESP32"; break;
        case CHIP_ESP32S2: chip_model = "ESP32-S2"; break;
        case CHIP_ESP32S3: chip_model = "ESP32-S3"; break;
        case CHIP_ESP32C3: chip_model = "ESP32-C3"; break;
        case CHIP_ESP32C2: chip_model = "ESP32-C2"; break;
        case CHIP_ESP32C6: chip_model = "ESP32-C6"; break;
        case CHIP_ESP32H2: chip_model = "ESP32-H2"; break;
        case CHIP_ESP32P4: chip_model = "ESP32-P4"; break;
    }

    ESP_LOGI(TAG, "Chip Model: %s", chip_model);
    ESP_LOGI(TAG, "CPU Cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "Revision: %d", chip_info.revision);
    ESP_LOGI(TAG, "WiFi: %s", (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "Yes" : "No");
    ESP_LOGI(TAG, "Bluetooth: %s", (chip_info.features & CHIP_FEATURE_BT) ? "Yes" : "No");
    ESP_LOGI(TAG, "Bluetooth LE: %s", (chip_info.features & CHIP_FEATURE_BLE) ? "Yes" : "No");
    ESP_LOGI(TAG, "IEEE 802.15.4: %s", (chip_info.features & CHIP_FEATURE_IEEE802154) ? "Yes" : "No");
    ESP_LOGI(TAG, "Embedded Flash: %s", (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "Yes" : "No");
    ESP_LOGI(TAG, "External PSRAM: %s", (chip_info.features & CHIP_FEATURE_EMB_PSRAM) ? "Yes" : "No");

    // IDF版本信息
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
}
```

### 内存监控系统

#### 堆内存统计
```cpp
size_t SystemInfo::GetFreeHeapSize() {
    return esp_get_free_heap_size();
}

size_t SystemInfo::GetMinimumFreeHeapSize() {
    return esp_get_minimum_free_heap_size();
}

void SystemInfo::PrintHeapStats() {
    // 内部SRAM统计
    int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    int total_sram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);

    ESP_LOGI(TAG, "=== Internal SRAM Stats ===");
    ESP_LOGI(TAG, "Free: %d bytes (%.1f KB)", free_sram, free_sram / 1024.0);
    ESP_LOGI(TAG, "Minimum free: %d bytes (%.1f KB)", min_free_sram, min_free_sram / 1024.0);
    ESP_LOGI(TAG, "Total: %d bytes (%.1f KB)", total_sram, total_sram / 1024.0);
    ESP_LOGI(TAG, "Usage: %.1f%%", ((total_sram - free_sram) * 100.0) / total_sram);

    // PSRAM统计 (如果可用)
    int free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (free_psram > 0) {
        int min_free_psram = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
        int total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

        ESP_LOGI(TAG, "=== External PSRAM Stats ===");
        ESP_LOGI(TAG, "Free: %d bytes (%.1f KB)", free_psram, free_psram / 1024.0);
        ESP_LOGI(TAG, "Minimum free: %d bytes (%.1f KB)", min_free_psram, min_free_psram / 1024.0);
        ESP_LOGI(TAG, "Total: %d bytes (%.1f KB)", total_psram, total_psram / 1024.0);
        ESP_LOGI(TAG, "Usage: %.1f%%", ((total_psram - free_psram) * 100.0) / total_psram);
    }

    // DMA内存统计
    int free_dma = heap_caps_get_free_size(MALLOC_CAP_DMA);
    int total_dma = heap_caps_get_total_size(MALLOC_CAP_DMA);

    ESP_LOGI(TAG, "=== DMA Memory Stats ===");
    ESP_LOGI(TAG, "Free: %d bytes (%.1f KB)", free_dma, free_dma / 1024.0);
    ESP_LOGI(TAG, "Total: %d bytes (%.1f KB)", total_dma, total_dma / 1024.0);
    ESP_LOGI(TAG, "Usage: %.1f%%", ((total_dma - free_dma) * 100.0) / total_dma);
}
```

#### 内存使用模式分析
```cpp
class MemoryAnalyzer {
public:
    struct MemorySnapshot {
        size_t total_free;
        size_t min_free;
        size_t largest_block;
        uint32_t timestamp;
    };

    static void TakeSnapshot(MemorySnapshot& snapshot) {
        snapshot.total_free = esp_get_free_heap_size();
        snapshot.min_free = esp_get_minimum_free_heap_size();
        snapshot.largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        snapshot.timestamp = esp_timer_get_time() / 1000;  // ms
    }

    static void AnalyzeFragmentation() {
        size_t total_free = esp_get_free_heap_size();
        size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

        if (total_free > 0) {
            float fragmentation = 1.0f - ((float)largest_block / total_free);
            ESP_LOGI(TAG, "Memory fragmentation: %.1f%%", fragmentation * 100);

            if (fragmentation > 0.5f) {
                ESP_LOGW(TAG, "High memory fragmentation detected!");
            }
        }
    }

    static void PrintMemoryMap() {
        ESP_LOGI(TAG, "=== Memory Capability Analysis ===");

        // 各种内存能力统计
        const malloc_caps_t capabilities[] = {
            MALLOC_CAP_EXEC,       // 可执行内存
            MALLOC_CAP_32BIT,      // 32位对齐
            MALLOC_CAP_8BIT,       // 8位访问
            MALLOC_CAP_DMA,        // DMA内存
            MALLOC_CAP_PID2,       // PID2内存
            MALLOC_CAP_PID3,       // PID3内存
            MALLOC_CAP_PID4,       // PID4内存
            MALLOC_CAP_PID5,       // PID5内存
            MALLOC_CAP_PID6,       // PID6内存
            MALLOC_CAP_PID7,       // PID7内存
            MALLOC_CAP_SPIRAM,     // PSRAM
            MALLOC_CAP_INTERNAL,   // 内部RAM
            MALLOC_CAP_DEFAULT     // 默认内存
        };

        const char* cap_names[] = {
            "EXEC", "32BIT", "8BIT", "DMA", "PID2", "PID3",
            "PID4", "PID5", "PID6", "PID7", "SPIRAM", "INTERNAL", "DEFAULT"
        };

        for (size_t i = 0; i < sizeof(capabilities) / sizeof(capabilities[0]); i++) {
            size_t free_size = heap_caps_get_free_size(capabilities[i]);
            size_t total_size = heap_caps_get_total_size(capabilities[i]);

            if (total_size > 0) {
                ESP_LOGI(TAG, "%s: %zu/%zu bytes (%.1f%% used)",
                         cap_names[i], total_size - free_size, total_size,
                         ((total_size - free_size) * 100.0) / total_size);
            }
        }
    }
};
```

### 任务监控和CPU分析

#### 任务列表打印
```cpp
void SystemInfo::PrintTaskList() {
    // 获取任务列表缓冲区大小
    UBaseType_t task_count = uxTaskGetNumberOfTasks();

    // 分配足够的缓冲区 (每个任务约40字节)
    size_t buffer_size = task_count * 40 + 100;
    char* buffer = (char*)malloc(buffer_size);

    if (buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for task list");
        return;
    }

    // 获取任务列表
    vTaskList(buffer);

    ESP_LOGI(TAG, "=== Task List ===");
    ESP_LOGI(TAG, "Name          State  Priority  Stack  Number");
    ESP_LOGI(TAG, "%s", buffer);

    free(buffer);

    // 任务状态解释
    ESP_LOGI(TAG, "State: R=Running, B=Blocked, S=Suspended, D=Deleted");
}

// 扩展: 任务详细信息
void PrintDetailedTaskInfo() {
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    TaskStatus_t* task_array = (TaskStatus_t*)malloc(task_count * sizeof(TaskStatus_t));

    if (task_array == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for task array");
        return;
    }

    // 获取任务状态
    UBaseType_t actual_count = uxTaskGetSystemState(task_array, task_count, nullptr);

    ESP_LOGI(TAG, "=== Detailed Task Information ===");
    for (UBaseType_t i = 0; i < actual_count; i++) {
        TaskStatus_t& task = task_array[i];

        const char* state_str = "Unknown";
        switch (task.eCurrentState) {
            case eRunning:   state_str = "Running"; break;
            case eReady:     state_str = "Ready"; break;
            case eBlocked:   state_str = "Blocked"; break;
            case eSuspended: state_str = "Suspended"; break;
            case eDeleted:   state_str = "Deleted"; break;
        }

        ESP_LOGI(TAG, "Task: %-16s | State: %-9s | Priority: %2lu | Stack HWM: %4u | Core: %d",
                 task.pcTaskName, state_str, task.uxCurrentPriority,
                 task.usStackHighWaterMark, task.xCoreID);
    }

    free(task_array);
}
```

#### CPU使用率分析
```cpp
esp_err_t SystemInfo::PrintTaskCpuUsage(TickType_t xTicksToWait) {
    const int ARRAY_SIZE_OFFSET = 5;
    TaskStatus_t *start_array = nullptr, *end_array = nullptr;
    UBaseType_t start_array_size, end_array_size;
    configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;
    esp_err_t ret = ESP_OK;

    // 第一次采样 - 获取起始状态
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = (TaskStatus_t*)malloc(sizeof(TaskStatus_t) * start_array_size);

    if (start_array == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for start array");
        return ESP_ERR_NO_MEM;
    }

    // 获取当前任务状态和运行时间
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ESP_LOGE(TAG, "Failed to get system state");
        ret = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }

    // 等待指定时间
    vTaskDelay(xTicksToWait);

    // 第二次采样 - 获取结束状态
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = (TaskStatus_t*)malloc(sizeof(TaskStatus_t) * end_array_size);

    if (end_array == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for end array");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ESP_LOGE(TAG, "Failed to get system state");
        ret = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }

    // 计算总运行时间
    uint32_t total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ESP_LOGE(TAG, "Invalid elapsed time");
        ret = ESP_ERR_INVALID_STATE;
        goto cleanup;
    }

    ESP_LOGI(TAG, "=== CPU Usage Analysis (Sample period: %lu ticks) ===", xTicksToWait);
    ESP_LOGI(TAG, "| %-16s | %8s | %8s | %4s", "Task", "Runtime", "Delta", "CPU%");
    ESP_LOGI(TAG, "|------------------|----------|----------|------|");

    // 匹配任务并计算CPU使用率
    uint32_t total_task_time = 0;

    for (UBaseType_t i = 0; i < start_array_size; i++) {
        // 查找匹配的任务
        int matching_index = -1;
        for (UBaseType_t j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                matching_index = j;
                break;
            }
        }

        if (matching_index >= 0) {
            // 计算该任务的运行时间增量
            uint32_t task_elapsed_time = end_array[matching_index].ulRunTimeCounter -
                                       start_array[i].ulRunTimeCounter;

            // 计算CPU使用百分比 (考虑多核)
            uint32_t percentage = (task_elapsed_time * 100UL) /
                                 (total_elapsed_time * CONFIG_FREERTOS_NUMBER_OF_CORES);

            total_task_time += task_elapsed_time;

            ESP_LOGI(TAG, "| %-16s | %8lu | %8lu | %3lu%% |",
                     start_array[i].pcTaskName,
                     end_array[matching_index].ulRunTimeCounter,
                     task_elapsed_time,
                     percentage);
        }
    }

    // 计算空闲时间百分比
    uint32_t idle_percentage = 100 - ((total_task_time * 100) /
                                     (total_elapsed_time * CONFIG_FREERTOS_NUMBER_OF_CORES));

    ESP_LOGI(TAG, "|------------------|----------|----------|------|");
    ESP_LOGI(TAG, "| %-16s |          |          | %3lu%% |", "IDLE", idle_percentage);

cleanup:
    if (start_array) free(start_array);
    if (end_array) free(end_array);

    return ret;
}
```

## 🔧 扩展功能和工具

### 性能监控器
```cpp
class PerformanceMonitor {
private:
    struct SystemMetrics {
        size_t free_heap;
        size_t min_free_heap;
        uint32_t task_count;
        uint32_t timestamp;
        float cpu_usage[CONFIG_FREERTOS_NUMBER_OF_CORES];
    };

    std::vector<SystemMetrics> history_;
    size_t max_history_size_ = 100;

public:
    void StartMonitoring(int interval_ms = 5000) {
        xTaskCreate([](void* param) {
            auto* monitor = static_cast<PerformanceMonitor*>(param);
            monitor->MonitoringTask();
        }, "perf_monitor", 4096, this, 1, nullptr);
    }

private:
    void MonitoringTask() {
        while (true) {
            SystemMetrics metrics = {};
            CollectMetrics(metrics);

            // 添加到历史记录
            if (history_.size() >= max_history_size_) {
                history_.erase(history_.begin());
            }
            history_.push_back(metrics);

            // 检测异常
            DetectAnomalies(metrics);

            vTaskDelay(pdMS_TO_TICKS(5000));  // 5秒间隔
        }
    }

    void CollectMetrics(SystemMetrics& metrics) {
        metrics.free_heap = esp_get_free_heap_size();
        metrics.min_free_heap = esp_get_minimum_free_heap_size();
        metrics.task_count = uxTaskGetNumberOfTasks();
        metrics.timestamp = esp_timer_get_time() / 1000;

        // 简化的CPU使用率采集
        // 实际实现需要更复杂的采样逻辑
        for (int i = 0; i < CONFIG_FREERTOS_NUMBER_OF_CORES; i++) {
            metrics.cpu_usage[i] = GetCoreCpuUsage(i);
        }
    }

    void DetectAnomalies(const SystemMetrics& current) {
        // 内存泄漏检测
        if (current.free_heap < current.min_free_heap * 1.1) {
            ESP_LOGW(TAG, "Potential memory leak detected - free heap approaching minimum");
        }

        // 任务爆炸检测
        if (current.task_count > 50) {  // 阈值可配置
            ESP_LOGW(TAG, "High task count detected: %lu tasks", current.task_count);
        }

        // CPU使用率异常
        for (int i = 0; i < CONFIG_FREERTOS_NUMBER_OF_CORES; i++) {
            if (current.cpu_usage[i] > 95.0f) {
                ESP_LOGW(TAG, "High CPU usage on core %d: %.1f%%", i, current.cpu_usage[i]);
            }
        }
    }

    float GetCoreCpuUsage(int core_id) {
        // 简化实现，实际需要基于运行时统计
        return 0.0f;  // 占位符
    }
};
```

### 诊断工具集
```cpp
class SystemDiagnostics {
public:
    static void RunFullDiagnostics() {
        ESP_LOGI(TAG, "=== System Diagnostics Report ===");

        // 硬件信息
        ESP_LOGI(TAG, "\n--- Hardware Information ---");
        LogHardwareInfo();

        // 内存状态
        ESP_LOGI(TAG, "\n--- Memory Status ---");
        SystemInfo::PrintHeapStats();
        MemoryAnalyzer::PrintMemoryMap();
        MemoryAnalyzer::AnalyzeFragmentation();

        // 任务状态
        ESP_LOGI(TAG, "\n--- Task Status ---");
        SystemInfo::PrintTaskList();

        // 分区信息
        ESP_LOGI(TAG, "\n--- Partition Information ---");
        LogPartitionInfo();

        // 网络状态
        ESP_LOGI(TAG, "\n--- Network Status ---");
        LogNetworkStatus();

        // 系统健康检查
        ESP_LOGI(TAG, "\n--- Health Check ---");
        RunHealthCheck();
    }

private:
    static void LogHardwareInfo() {
        ESP_LOGI(TAG, "Chip: %s", SystemInfo::GetChipModelName().c_str());
        ESP_LOGI(TAG, "MAC: %s", SystemInfo::GetMacAddress().c_str());
        ESP_LOGI(TAG, "Flash: %zu bytes", SystemInfo::GetFlashSize());

        // 详细芯片信息
        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        ESP_LOGI(TAG, "Cores: %d, Revision: %d", chip_info.cores, chip_info.revision);
    }

    static void LogPartitionInfo() {
        esp_partition_iterator_t iterator = esp_partition_find(ESP_PARTITION_TYPE_ANY,
                                                              ESP_PARTITION_SUBTYPE_ANY,
                                                              nullptr);

        while (iterator != nullptr) {
            const esp_partition_t* partition = esp_partition_get(iterator);
            ESP_LOGI(TAG, "Partition: %-10s | Type: %2d | Subtype: %3d | Offset: 0x%06lX | Size: %6lu KB",
                     partition->label, partition->type, partition->subtype,
                     partition->address, partition->size / 1024);

            iterator = esp_partition_next(iterator);
        }

        esp_partition_iterator_release(iterator);

        // 当前运行分区
        const esp_partition_t* running = esp_ota_get_running_partition();
        ESP_LOGI(TAG, "Running from: %s", running ? running->label : "unknown");
    }

    static void LogNetworkStatus() {
        auto& board = Board::GetInstance();
        auto* network = board.GetNetwork();

        if (network) {
            if (network->IsConnected()) {
                ESP_LOGI(TAG, "Network: Connected (%s)", network->GetConnectionInfo().c_str());
            } else {
                ESP_LOGI(TAG, "Network: Disconnected");
            }
        } else {
            ESP_LOGI(TAG, "Network: Not available");
        }
    }

    static void RunHealthCheck() {
        bool healthy = true;

        // 内存健康检查
        size_t free_heap = esp_get_free_heap_size();
        size_t min_free = esp_get_minimum_free_heap_size();

        if (free_heap < 50 * 1024) {  // 少于50KB
            ESP_LOGW(TAG, "❌ Low memory warning: %zu bytes free", free_heap);
            healthy = false;
        } else {
            ESP_LOGI(TAG, "✅ Memory OK: %zu bytes free", free_heap);
        }

        // 任务数量检查
        UBaseType_t task_count = uxTaskGetNumberOfTasks();
        if (task_count > 30) {  // 超过30个任务
            ESP_LOGW(TAG, "❌ High task count: %lu tasks", task_count);
            healthy = false;
        } else {
            ESP_LOGI(TAG, "✅ Task count OK: %lu tasks", task_count);
        }

        // 网络连接检查
        auto& board = Board::GetInstance();
        auto* network = board.GetNetwork();
        if (network && network->IsConnected()) {
            ESP_LOGI(TAG, "✅ Network connection OK");
        } else {
            ESP_LOGW(TAG, "⚠️  Network disconnected");
        }

        ESP_LOGI(TAG, "\nOverall system health: %s", healthy ? "✅ HEALTHY" : "❌ ISSUES DETECTED");
    }
};
```

## 🔗 与其他模块集成

### MCP工具集成
```cpp
void RegisterSystemInfoMcpTools() {
    auto& mcp_server = McpServer::GetInstance();

    // 系统信息查询工具
    mcp_server.AddTool("get_system_info", "获取系统硬件和软件信息",
        PropertyList(), [](const PropertyList& properties) -> ReturnValue {
            cJSON* info = cJSON_CreateObject();

            // 硬件信息
            cJSON_AddStringToObject(info, "chip_model", SystemInfo::GetChipModelName().c_str());
            cJSON_AddStringToObject(info, "mac_address", SystemInfo::GetMacAddress().c_str());
            cJSON_AddNumberToObject(info, "flash_size", SystemInfo::GetFlashSize());

            // 内存信息
            cJSON_AddNumberToObject(info, "free_heap", SystemInfo::GetFreeHeapSize());
            cJSON_AddNumberToObject(info, "min_free_heap", SystemInfo::GetMinimumFreeHeapSize());

            // 任务信息
            cJSON_AddNumberToObject(info, "task_count", uxTaskGetNumberOfTasks());

            char* json_str = cJSON_PrintUnformatted(info);
            std::string result(json_str);
            cJSON_free(json_str);
            cJSON_Delete(info);

            return result;
        });

    // 内存统计工具
    mcp_server.AddTool("get_memory_stats", "获取详细内存使用统计",
        PropertyList(), [](const PropertyList& properties) -> ReturnValue {
            // 重定向日志输出到字符串
            std::stringstream ss;

            // 保存原始日志级别并临时重定向
            SystemInfo::PrintHeapStats();  // 直接调用，结果会输出到日志

            return std::string("内存统计已输出到日志");
        });

    // 系统诊断工具
    mcp_server.AddTool("run_diagnostics", "运行系统全面诊断",
        PropertyList(), [](const PropertyList& properties) -> ReturnValue {
            SystemDiagnostics::RunFullDiagnostics();
            return std::string("系统诊断已完成，结果已输出到日志");
        });
}
```

### 调试集成
```cpp
void SetupAdvancedLogging() {
    // 定期内存监控
    xTaskCreate([](void* param) {
        while (true) {
            static size_t last_free = 0;
            size_t current_free = esp_get_free_heap_size();

            if (last_free > 0) {
                int diff = current_free - last_free;
                if (abs(diff) > 1024) {  // 变化超过1KB
                    ESP_LOGI(TAG, "Memory change: %+d bytes (now: %zu)", diff, current_free);
                }
            }

            last_free = current_free;
            vTaskDelay(pdMS_TO_TICKS(10000));  // 10秒
        }
    }, "mem_monitor", 2048, nullptr, 1, nullptr);

    // 崩溃信息收集
    esp_err_t err = esp_core_dump_init();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Core dump initialized for crash analysis");
    }
}
```

---

**相关文档**:
- [Application主控制器](../02-main-core/02-application-class.md)
- [Board硬件抽象层](../05-board-abstraction/01-board-base.md)
- [MCP工具服务器](./03-mcp-server.md)
- [OTA升级系统](./05-ota-system.md)
