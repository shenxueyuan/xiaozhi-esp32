# main.cc - 应用程序入口点

## 📁 文件信息
- **路径**: `main/main.cc`
- **类型**: 应用程序入口
- **语言**: C++
- **功能**: ESP32应用程序的入口点和初始化

## 🎯 核心功能

### 主要函数
```cpp
extern "C" void app_main(void)
```

### 功能描述
作为整个xiaozhi-esp32应用的入口点，负责：
1. 初始化ESP-IDF默认事件循环
2. 初始化NVS（非易失性存储）
3. 启动应用程序主控制器

## 🔗 关键依赖

### 头文件依赖
```cpp
#include <esp_log.h>        // ESP-IDF日志系统
#include <esp_err.h>        // ESP-IDF错误码
#include <nvs.h>           // NVS存储
#include <nvs_flash.h>     // NVS Flash操作
#include <driver/gpio.h>   // GPIO驱动
#include <esp_event.h>     // ESP事件系统

#include "application.h"   // 应用主控制器
#include "system_info.h"   // 系统信息
```

### 调用关系
- **被调用**: ESP-IDF系统自动调用
- **调用**: `Application::GetInstance()` → `Application::Start()` → `Application::MainEventLoop()`

## 📋 执行流程

### 初始化序列
1. **事件循环初始化**
   ```cpp
   ESP_ERROR_CHECK(esp_event_loop_create_default());
   ```

2. **NVS Flash初始化**
   ```cpp
   esp_err_t ret = nvs_flash_init();
   if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
       ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
       ESP_ERROR_CHECK(nvs_flash_erase());
       ret = nvs_flash_init();
   }
   ```

3. **应用程序启动**
   ```cpp
   auto& app = Application::GetInstance();
   app.Start();
   app.MainEventLoop();
   ```

## 🚀 重要特性

### 错误处理
- 自动检测和修复NVS Flash损坏
- 使用ESP_ERROR_CHECK确保初始化成功

### 设计模式
- 单例模式：Application使用单例模式确保全局唯一实例
- 事件驱动：基于ESP-IDF事件循环的异步架构

## 🔄 生命周期

1. **系统启动** → ESP-IDF调用app_main()
2. **初始化阶段** → 设置基础系统服务
3. **应用启动** → 启动Application实例
4. **事件循环** → 进入主事件循环，不再返回

## 📊 性能考虑

- **内存使用**: 最小化启动阶段内存使用
- **启动时间**: 快速完成基础初始化
- **稳定性**: 包含错误恢复机制

## 🔧 配置相关

### NVS配置
- 用于存储WiFi配置、设备设置等持久化数据
- 支持自动错误恢复和重置

### 事件系统
- 为整个应用提供异步事件处理基础
- 支持跨任务、跨模块的事件通信

---

**下一步**: 分析 `Application` 类的实现
**相关文档**: [Application类分析](./02-application-class.md)
