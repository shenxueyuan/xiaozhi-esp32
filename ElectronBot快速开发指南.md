# ElectronBot 快速开发指南

## 1. 开发环境准备

ElectronBot基于ESP32-S3平台开发，使用ESP-IDF框架。要开始开发，需要准备以下环境：

1. 安装ESP-IDF开发环境
2. 克隆项目代码库
3. 连接ElectronBot硬件

## 2. 项目结构

ElectronBot的代码位于项目的`main/boards/electron-bot/`目录下，主要包含以下文件：

- `electron_bot.cc`: 主板类实现
- `electron_bot_controller.cc`: 控制器实现
- `movements.h/cc`: 动作控制逻辑
- `oscillator.h/cc`: 舵机控制基础类
- `electron_emoji_display.h/cc`: 表情显示实现
- `power_manager.h`: 电源管理实现
- `config.h`: 硬件配置
- `config.json`: 构建配置

## 3. 快速开发指南

### 3.1 添加新动作

要添加新的动作，可以在`movements.cc`中扩展现有的动作函数，或者添加新的动作函数。

例如，添加一个新的手部动作：

```cpp
// 在Otto类中添加新方法
void Otto::HandWavePattern(int times, int amount, int period) {
    // 实现复杂的手部挥动模式
    int current_positions[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        current_positions[i] = (servo_pins_[i] != -1) ? servo_[i].GetPosition() : servo_initial_[i];
    }

    // 实现特定的动作序列
    // ...

    // 回到初始位置
    memcpy(current_positions, servo_initial_, sizeof(current_positions));
    MoveServos(period, current_positions);
}
```

然后在`electron_bot_controller.cc`中注册对应的MCP工具：

```cpp
// 在RegisterMcpTools方法中添加
mcp_server.AddTool(
    "self.electron.hand_wave_pattern",
    "复杂的手部挥动模式",
    PropertyList({
        Property("times", kPropertyTypeInteger, 1, 1, 10),
        Property("amount", kPropertyTypeInteger, 30, 10, 50),
        Property("period", kPropertyTypeInteger, 1000, 500, 2000)
    }),
    [this](const PropertyList& properties) -> ReturnValue {
        int times = properties["times"].value<int>();
        int amount = properties["amount"].value<int>();
        int period = properties["period"].value<int>();

        electron_bot_.HandWavePattern(times, amount, period);
        return true;
    });
```

### 3.2 修改表情映射

要添加或修改表情映射，可以编辑`electron_emoji_display.cc`中的`emotion_maps_`数组：

```cpp
// 添加新的表情映射
const ElectronEmojiDisplay::EmotionMap ElectronEmojiDisplay::emotion_maps_[] = {
    // 已有映射...

    // 添加新映射
    {"excited", &happy},
    {"joyful", &happy},
    {"curious", &buxue},

    {nullptr, nullptr}  // 结束标记
};
```

### 3.3 调整舵机参数

如果需要调整舵机的默认参数，可以修改`movements.h`中的`servo_initial_`数组：

```cpp
int servo_initial_[SERVO_COUNT] = {180, 180, 0, 0, 90, 90};
```

或者通过MCP工具`self.electron.set_trim`在运行时调整舵机微调参数。

### 3.4 添加新的MCP工具

要添加新的控制功能，可以在`electron_bot_controller.cc`的`RegisterMcpTools`方法中添加新的MCP工具：

```cpp
// 添加新的MCP工具
mcp_server.AddTool(
    "self.electron.custom_action",
    "自定义动作组合",
    PropertyList({
        Property("param1", kPropertyTypeInteger, 1, 1, 10),
        Property("param2", kPropertyTypeString, "default")
    }),
    [this](const PropertyList& properties) -> ReturnValue {
        int param1 = properties["param1"].value<int>();
        std::string param2 = properties["param2"].value<std::string>();

        // 实现自定义动作
        // ...

        return "动作执行成功";
    });
```

## 4. 舵机控制详解

### 4.1 基础舵机控制

ElectronBot使用`Oscillator`类控制舵机，主要方法包括：

- `Attach(pin, rev)`: 连接舵机到指定引脚
- `Detach()`: 断开舵机连接
- `SetPosition(position)`: 设置舵机位置（0-180度）
- `SetTrim(trim)`: 设置舵机微调值

### 4.2 高级舵机控制

`Oscillator`类还支持振荡控制，用于实现复杂的动作：

- `SetA(amplitude)`: 设置振荡幅度
- `SetO(offset)`: 设置振荡偏移
- `SetPh(phase)`: 设置振荡相位
- `SetT(period)`: 设置振荡周期

使用示例：

```cpp
void Otto::CustomOscillation() {
    int amplitude[SERVO_COUNT] = {30, 30, 30, 30, 0, 0};  // 振幅
    int offset[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};   // 偏移
    double phase_diff[SERVO_COUNT] = {0, DEG2RAD(90), DEG2RAD(180), DEG2RAD(270), 0, 0}; // 相位差
    int period = 1000;  // 周期(毫秒)

    OscillateServos(amplitude, offset, period, phase_diff, 3.0); // 执行3个周期
}
```

## 5. 表情显示详解

ElectronBot使用LVGL库显示GIF表情，主要实现在`ElectronEmojiDisplay`类中：

### 5.1 显示表情

```cpp
// 设置表情
display->SetEmotion("happy");  // 显示开心表情
display->SetEmotion("sad");    // 显示悲伤表情
display->SetEmotion("angry");  // 显示愤怒表情
```

### 5.2 显示聊天消息

```cpp
// 显示聊天消息
display->SetChatMessage("user", "你好，ElectronBot！");
```

### 5.3 添加自定义GIF表情

要添加新的GIF表情，需要：

1. 将GIF转换为LVGL兼容的C数组
2. 在项目中添加表情声明
3. 更新表情映射表

## 6. 电源管理详解

ElectronBot的电源管理通过`PowerManager`类实现，主要功能包括：

### 6.1 电池电量检测

```cpp
// 获取电池电量
int level = power_manager->GetBatteryLevel();  // 返回0-100的电量百分比
```

### 6.2 充电状态检测

```cpp
// 获取充电状态
bool charging = power_manager->IsCharging();  // 返回true表示正在充电
```

## 7. 常见问题解决

### 7.1 舵机校准问题

如果舵机初始位置不准确，可以通过以下方式校准：

1. 使用MCP工具`self.electron.set_trim`设置舵机微调参数
2. 修改`movements.h`中的`servo_initial_`数组

### 7.2 表情显示问题

如果表情显示异常，可以检查：

1. GIF文件是否正确转换
2. 表情映射是否正确
3. 显示初始化是否成功

### 7.3 动作执行问题

如果动作执行异常，可以检查：

1. 舵机连接是否正确
2. 动作参数是否在合理范围内
3. 电池电量是否充足

## 8. 性能优化建议

### 8.1 舵机控制优化

- 减少不必要的舵机移动
- 使用合适的移动速度，避免过快导致电流过大
- 动作结束后考虑断开舵机连接以节省电量

### 8.2 表情显示优化

- 使用适当大小的GIF文件，避免过大占用内存
- 减少不必要的表情切换
- 考虑使用静态图片替代部分GIF动画

### 8.3 电源管理优化

- 实现低电量模式，减少动作幅度和频率
- 长时间不使用时自动进入休眠模式
- 优化ADC读取频率，减少不必要的电量检测
