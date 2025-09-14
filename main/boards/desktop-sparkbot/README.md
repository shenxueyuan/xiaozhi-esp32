# Desktop SparkBot 桌面机器人

基于ESP-SparkBot扩展的桌面小机器人，支持全屏表情显示和减速电机动作控制。

## 🎯 功能特性

- ✅ **全屏表情显示** - 240x240全屏GIF表情动画
- ✅ **双轴电机控制** - 头部上下 + 身体左右转动
- ✅ **情绪驱动动作** - 根据情绪自动执行相应动作
- ✅ **完全兼容** - 保持所有ESP-SparkBot原有功能
- ✅ **MCP工具集成** - 支持远程控制和配置
- ✅ **可配置参数** - 动作强度、速度、延迟可调

## 🔧 硬件配置

### 基础硬件 (继承自ESP-SparkBot)
- **主控**: ESP32-S3
- **显示**: 240x240 ST7789 LCD
- **音频**: ES8311编解码器
- **摄像头**: OV3660
- **存储**: 16MB Flash + 8MB PSRAM

### 新增硬件
- **头部电机**: 步进电机 + 驱动模块 (GPIO 1,2)
- **身体电机**: 步进电机 + 驱动模块 (GPIO 14,3)

## 📋 引脚分配

| 功能 | GPIO | 备注 |
|------|------|------|
| 头部电机引脚1 | GPIO_1 | 步进控制信号1 |
| 头部电机引脚2 | GPIO_2 | 步进控制信号2 |
| 身体电机引脚1 | GPIO_14 | 步进控制信号1 |
| 身体电机引脚2 | GPIO_3 | 步进控制信号2 |

*其他引脚保持与ESP-SparkBot相同*

## 🎭 表情动作映射

| 情绪 | 表情GIF | 动作响应 | 延迟 |
|------|---------|----------|------|
| happy | 开心动画 | 头部上扬 + 身体摇摆 | 500ms |
| sad | 悲伤动画 | 头部下垂 | 800ms |
| angry | 愤怒动画 | 头部摇动 + 身体快摇 | 200ms |
| surprised | 惊讶动画 | 头部快速上扬 | 100ms |
| thinking | 思考动画 | 头部轻微点头 | 1000ms |
| neutral | 中性动画 | 回到中心位置 | 1500ms |

## 🚀 编译和烧录

### 1. 环境准备
```bash
# 确保ESP-IDF环境已配置
source ~/esp/esp-idf/export.sh
```

### 2. 配置和编译
```bash
cd xiaozhi-esp32
git checkout feature/desktop-sparkbot-robot

# 配置目标芯片
idf.py set-target esp32s3

# 配置板卡 (如果有menuconfig选项)
idf.py menuconfig
# Board selection -> desktop-sparkbot

# 编译
idf.py build
```

### 3. 烧录
```bash
# 烧录固件
idf.py flash

# 监控串口输出
idf.py monitor
```

## 🧪 测试指南

### 基础电机测试
```bash
# 头部上下转动
{
  "type": "mcp",
  "payload": {
    "method": "tools/call",
    "params": {
      "name": "self.head.up_down",
      "arguments": {"angle": 30, "speed": 2}
    }
  }
}

# 身体左右转动
{
  "type": "mcp",
  "payload": {
    "method": "tools/call",
    "params": {
      "name": "self.body.left_right",
      "arguments": {"angle": -45, "speed": 1}
    }
  }
}
```

### 表情动作联动测试
```bash
# 开心表情 + 动作
{
  "type": "llm",
  "emotion": "happy"
}

# 愤怒表情 + 动作
{
  "type": "llm",
  "emotion": "angry"
}
```

### 配置测试
```bash
# 调整动作强度
{
  "type": "mcp",
  "payload": {
    "method": "tools/call",
    "params": {
      "name": "self.desktop.motion_config",
      "arguments": {
        "enabled": true,
        "intensity_scale": 0.8
      }
    }
  }
}
```

## ⚠️ 注意事项

### 硬件要求
1. **电源供应** - 减速电机功耗较大，建议使用5V/2A以上电源
2. **电机驱动** - 推荐使用L298N或TB6612等电机驱动模块
3. **GPIO冲突** - 确认新增GPIO不与现有功能冲突
4. **机械限位** - 建议添加限位开关防止电机过转

### 软件限制
1. **角度控制** - 当前使用时间估算，建议添加编码器反馈
2. **内存使用** - 全屏GIF占用较多内存，注意监控
3. **任务优先级** - 电机任务优先级为5，可根据需要调整

## 🔧 自定义配置

### 修改电机参数
编辑 `config.h` 文件：
```cpp
#define MOTOR_DEFAULT_SPEED    400   // 调整默认速度
#define HEAD_MAX_ANGLE         45    // 调整角度限制
#define BODY_MAX_ANGLE         90    // 调整角度限制
```

### 修改表情映射
编辑 `emotion_action_controller.cc` 中的 `emotion_action_rules_` 数组。

### 添加新动作
在 `MotorController` 类中添加新的动作函数，并在MCP工具中注册。

## 📞 技术支持

如有问题请参考：
1. [完整实现方案文档](../../code-analysis/desktop-robot-implementation-plan.md)
2. [原ESP-SparkBot配置](../esp-sparkbot/)
3. [项目代码分析](../../code-analysis/)

---

*基于 xiaozhi-esp32 v1.8.8 开发*
