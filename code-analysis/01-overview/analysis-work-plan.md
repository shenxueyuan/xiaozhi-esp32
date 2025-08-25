# xiaozhi-esp32 代码分析工作计划

## 📊 文件统计

总计 **370 个源文件**，按分类统计：

### 🎯 核心应用层 (7 个文件)
- `main/main.cc` - 应用程序入口
- `main/application.h` + `main/application.cc` - 应用主控制器
- `main/device_state.h` + `main/device_state_event.h` + `main/device_state_event.cc` - 设备状态管理
- `main/system_info.h` + `main/system_info.cc` - 系统信息

### 🎵 音频系统 (35 个文件)
#### 音频服务核心
- `main/audio/audio_service.h` + `main/audio/audio_service.cc` - 音频服务主控
- `main/audio/audio_codec.h` + `main/audio/audio_codec.cc` - 音频编解码器抽象
- `main/audio/audio_processor.h` - 音频处理器抽象

#### 音频编解码器 (16 个文件)
- ES8311 系列：`main/audio/codecs/es8311_audio_codec.h/cc`
- ES8374 系列：`main/audio/codecs/es8374_audio_codec.h/cc`
- ES8388 系列：`main/audio/codecs/es8388_audio_codec.h/cc`
- ES8389 系列：`main/audio/codecs/es8389_audio_codec.h/cc`
- Box 系列：`main/audio/codecs/box_audio_codec.h/cc`
- 无音频：`main/audio/codecs/no_audio_codec.h/cc`
- 虚拟音频：`main/audio/codecs/dummy_audio_codec.h/cc`

#### 音频处理器 (8 个文件)
- AFE 处理器：`main/audio/processors/afe_audio_processor.h/cc`
- 无处理器：`main/audio/processors/no_audio_processor.h/cc`
- 音频调试器：`main/audio/processors/audio_debugger.h/cc`

#### 唤醒词检测 (7 个文件)
- 唤醒词抽象：`main/audio/wake_word.h`
- AFE 唤醒词：`main/audio/wake_words/afe_wake_word.h/cc`
- ESP 唤醒词：`main/audio/wake_words/esp_wake_word.h/cc`
- 自定义唤醒词：`main/audio/wake_words/custom_wake_word.h/cc`

### 🖥️ 显示系统 (8 个文件)
- 显示抽象：`main/display/display.h/cc`
- LCD 显示：`main/display/lcd_display.h/cc`
- OLED 显示：`main/display/oled_display.h/cc`
- 日志显示：`main/display/esplog_display.h/cc`

### 🔌 硬件抽象层 (298 个文件)
#### 通用组件 (36 个文件)
- Board 基类：`main/boards/common/board.h/cc`
- WiFi 板：`main/boards/common/wifi_board.h/cc`
- 双网络板：`main/boards/common/dual_network_board.h/cc`
- ML307 板：`main/boards/common/ml307_board.h/cc`
- 按钮控制：`main/boards/common/button.h/cc`
- 背光控制：`main/boards/common/backlight.h/cc`
- 摄像头：`main/boards/common/esp32_camera.h/cc` + `main/boards/common/camera.h`
- 电池监控：`main/boards/common/adc_battery_monitor.h/cc`
- 电源管理：`main/boards/common/power_save_timer.h/cc` + `main/boards/common/sleep_timer.h/cc`
- 系统重置：`main/boards/common/system_reset.h/cc`
- I2C 设备：`main/boards/common/i2c_device.h/cc`
- 其他外设...

#### 具体开发板 (262 个文件)
按厂商分类：
- **ESP 官方**：ESP-Box 系列、ESP-HI、Korvo2 等
- **M5Stack**：Core S3、Tab5、Atom 系列
- **教育厂商**：LabPlus、Kevin 系列、立创等
- **商业厂商**：LilyGo、DFRobot、Waveshare 等
- **机器人**：ESP-SparkBot、Otto Robot、Electron Bot
- **其他专用板**

### 💡 LED 控制 (6 个文件)
- LED 抽象：`main/led/led.h`
- 单个 LED：`main/led/single_led.h/cc`
- 环形灯带：`main/led/circular_strip.h/cc`
- GPIO LED：`main/led/gpio_led.h/cc`

### 🌐 通信协议 (6 个文件)
- 协议抽象：`main/protocols/protocol.h/cc`
- MQTT 协议：`main/protocols/mqtt_protocol.h/cc`
- WebSocket 协议：`main/protocols/websocket_protocol.h/cc`

### 🛠️ 工具和辅助 (10 个文件)
- MCP 服务器：`main/mcp_server.h/cc`
- OTA 更新：`main/ota.h/cc`
- 设置管理：`main/settings.h/cc`
- 系统信息：`main/system_info.h/cc`
- 语言配置：`main/assets/lang_config.h`
- Emoji 生成：`main/mmap_generate_emoji.h`

## 📋 分析优先级

### 高优先级（核心架构）
1. **应用主控**：`Application` 类
2. **硬件抽象**：`Board` 基类及实现
3. **音频服务**：`AudioService` 类
4. **通信协议**：`Protocol` 基类及实现

### 中优先级（重要功能）
1. **音频编解码器**：各种 AudioCodec 实现
2. **显示系统**：Display 抽象及实现
3. **设备状态管理**：DeviceState 相关
4. **MCP 服务器**：工具集成

### 低优先级（具体实现）
1. **具体开发板**：各厂商开发板实现
2. **特定功能**：LED、按钮等外设
3. **工具类**：设置、OTA 等

## 🔄 重要流程时序图计划

1. **应用启动流程**：从 main() 到 Application::Start()
2. **音频处理流程**：音频采集→处理→编码→发送
3. **音频播放流程**：网络接收→解码→播放
4. **唤醒词检测流程**：音频输入→检测→回调
5. **设备状态转换**：各状态间的转换逻辑
6. **网络通信流程**：MQTT/WebSocket 通信
7. **OTA 更新流程**：检查→下载→安装→重启
8. **开发板初始化**：硬件抽象层初始化过程

---

*总计：370 个文件，按模块分类进行系统性分析*
