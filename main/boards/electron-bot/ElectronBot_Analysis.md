# ElectronBot - 桌面级智能机器人详解

## 🎯 核心定位

**ElectronBot**是一款桌面级智能机器人，由稚晖君开源设计，外观灵感来源于WALL-E中的EVE。具备6个自由度关节控制、丰富的表情显示和智能交互能力，是一个集成了多种AI功能的桌面伴侣机器人。

---

## 🔧 硬件架构

### 主控平台
- **处理器**: ESP32-S3
- **显示屏**: 240x240 圆形 GC9A01 SPI LCD
- **关节控制**: 6个特制舵机 (支持角度回传)
- **通信**: WiFi + USB
- **电源**: 电池供电 + 充电检测

### 关节配置 (6自由度)
```cpp
// 舵机引脚配置
#define Right_Pitch_Pin GPIO_NUM_5  // 右手俯仰
#define Right_Roll_Pin GPIO_NUM_4   // 右手翻滚
#define Left_Pitch_Pin GPIO_NUM_7   // 左手俯仰
#define Left_Roll_Pin GPIO_NUM_15   // 左手翻滚
#define Body_Pin GPIO_NUM_6         // 身体旋转
#define Head_Pin GPIO_NUM_16        // 头部俯仰
```

### 外设接口
- **音频**: I2S麦克风 + 扬声器
- **显示**: SPI LCD 显示屏
- **通信**: WiFi + MCP协议
- **电源**: ADC电池电压检测

---

## 🤖 机器人控制系统

### 关节控制架构
- **舵机类型**: 特制舵机 (支持角度回传)
- **控制精度**: 角度反馈控制
- **运动范围**: 各关节独立控制
- **同步控制**: 多关节协同运动

### 动作分类系统

#### 手部动作 (12种)
```cpp
enum ActionType {
    ACTION_HAND_LEFT_UP = 1,      // 举左手
    ACTION_HAND_RIGHT_UP = 2,     // 举右手
    ACTION_HAND_BOTH_UP = 3,      // 举双手
    ACTION_HAND_LEFT_DOWN = 4,    // 放左手
    ACTION_HAND_RIGHT_DOWN = 5,   // 放右手
    ACTION_HAND_BOTH_DOWN = 6,    // 放双手
    ACTION_HAND_LEFT_WAVE = 7,    // 挥左手
    ACTION_HAND_RIGHT_WAVE = 8,   // 挥右手
    ACTION_HAND_BOTH_WAVE = 9,    // 挥双手
    ACTION_HAND_LEFT_FLAP = 10,   // 拍打左手
    ACTION_HAND_RIGHT_FLAP = 11,  // 拍打右手
    ACTION_HAND_BOTH_FLAP = 12,   // 拍打双手
};
```

#### 身体动作 (3种)
```cpp
ACTION_BODY_TURN_LEFT = 13,    // 左转
ACTION_BODY_TURN_RIGHT = 14,   // 右转
ACTION_BODY_TURN_CENTER = 15,  // 回中心
```

#### 头部动作 (5种)
```cpp
ACTION_HEAD_UP = 16,          // 抬头
ACTION_HEAD_DOWN = 17,        // 低头
ACTION_HEAD_NOD_ONCE = 18,    // 点头一次
ACTION_HEAD_CENTER = 19,      // 回中心
ACTION_HEAD_NOD_REPEAT = 20,  // 连续点头
```

#### 系统动作 (1种)
```cpp
ACTION_HOME = 21  // 复位到初始位置
```

---

## 🎭 表情显示系统

### GIF表情库
- **表情数量**: 6个基础GIF表情
- **显示尺寸**: 240x240 全屏显示
- **动画格式**: LVGL GIF格式

### 表情映射表
```cpp
// 表情映射到GIF资源
const EmotionMap emotion_maps_[] = {
    // 中性/平静类 -> staticstate
    {"neutral", &staticstate},
    {"relaxed", &staticstate},
    {"sleepy", &staticstate},

    // 积极/开心类 -> happy
    {"happy", &happy},
    {"laughing", &happy},
    {"funny", &happy},
    {"loving", &happy},
    {"confident", &happy},
    {"winking", &happy},
    {"cool", &happy},
    {"delicious", &happy},
    {"kissy", &happy},
    {"silly", &happy},

    // 悲伤类 -> sad
    {"sad", &sad},
    {"crying", &sad},

    // 愤怒类 -> anger
    {"angry", &anger},

    // 惊讶类 -> scare
    {"surprised", &scare},
    {"shocked", &scare},

    // 思考/困惑类 -> buxue
    {"thinking", &buxue},
    {"confused", &buxue},
    {"embarrassed", &buxue},
};
```

### 表情资源声明
```cpp
// GIF资源声明
LV_IMAGE_DECLARE(staticstate);  // 静态状态/中性表情
LV_IMAGE_DECLARE(sad);          // 悲伤
LV_IMAGE_DECLARE(happy);        // 开心
LV_IMAGE_DECLARE(scare);        // 惊吓/惊讶
LV_IMAGE_DECLARE(buxue);        // 不学/困惑
LV_IMAGE_DECLARE(anger);        // 愤怒
```

---

## ⚙️ 动作控制系统

### 运动学控制
- **振荡器系统**: 基于正弦波的平滑运动
- **相位差控制**: 多关节协调运动
- **速度控制**: 可调节运动速度
- **角度限制**: 关节角度保护

### 动作参数配置
```cpp
struct ElectronBotActionParams {
    int action_type;  // 动作类型
    int steps;        // 执行次数
    int speed;        // 运动速度
    int direction;    // 运动方向
    int amount;       // 运动幅度
};
```

### 队列执行系统
- **动作排队**: 支持动作序列执行
- **并发控制**: 单动作执行锁
- **状态监控**: 实时动作执行状态
- **错误处理**: 动作执行异常处理

---

## 🔋 电源管理系统

### 电池监控
```cpp
class PowerManager {
    // 电池电量区间-分压电阻为2个100k
    static constexpr struct {
        uint16_t adc;
        uint8_t level;
    } BATTERY_LEVELS[] = {{2150, 0}, {2450, 100}};
};
```

### 充电检测
- **充电状态**: GPIO检测充电状态
- **电量计算**: ADC采样 + 平均滤波
- **低电量保护**: 自动关机保护
- **状态上报**: 实时电量状态

### 电源配置
```cpp
#define POWER_CHARGE_DETECT_PIN GPIO_NUM_14
#define POWER_ADC_UNIT ADC_UNIT_1
#define POWER_ADC_CHANNEL ADC_CHANNEL_2
```

---

## 🌐 MCP协议控制

### 控制接口
- **协议标准**: MCP (Model Context Protocol)
- **动作控制**: 远程动作指令执行
- **状态查询**: 实时状态反馈
- **批量控制**: 动作序列控制

### AI指令示例

#### 手部动作
- "举起双手"
- "挥挥手"
- "拍拍手"
- "放下手臂"

#### 身体动作
- "向左转30度"
- "向右转45度"
- "转个身"

#### 头部动作
- "抬头看看"
- "低头思考"
- "点点头"
- "连续点头表示同意"

#### 组合动作
- "挥手告别" (挥手 + 点头)
- "表示同意" (点头 + 举手)
- "环顾四周" (左转 + 右转)

---

## 🎨 显示系统

### LCD显示配置
```cpp
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SWAP_XY false
```

### UI组件
- **表情GIF**: 全屏表情动画
- **聊天消息**: 滚动文本显示
- **状态图标**: 系统状态指示
- **背景**: 透明背景设计

---

## 🔊 音频系统

### 音频配置
```cpp
#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_I2S_METHOD_SIMPLEX

// 麦克风配置
#define AUDIO_I2S_MIC_GPIO_WS GPIO_NUM_40
#define AUDIO_I2S_MIC_GPIO_SCK GPIO_NUM_42
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_41

// 扬声器配置
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_17
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_18
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_8
```

---

## 📁 软件架构

### 核心文件结构
```
electron-bot/
├── electron_bot.cc           # 主板实现
├── config.h                  # 硬件配置
├── electron_bot_controller.cc # MCP控制器
├── electron_emoji_display.cc/h # 表情显示
├── movements.cc/h            # 运动控制
├── oscillator.cc/h           # 振荡器
├── power_manager.h           # 电源管理
├── README.md                 # 项目说明
└── config.json               # 编译配置
```

### 控制流程
1. **指令接收**: MCP协议接收控制指令
2. **动作解析**: 解析动作参数和类型
3. **队列管理**: 将动作加入执行队列
4. **运动执行**: 舵机控制执行动作
5. **状态反馈**: 实时状态上报

---

## 🎯 应用场景

### 智能桌面助手
- **AI交互**: 语音指令控制
- **表情反馈**: 情感化交互体验
- **动作演示**: 物理动作配合对话

### 教育机器人
- **编程教育**: 机器人编程学习
- **互动教学**: 生动教学演示
- **STEM教育**: 科技教育应用

### 娱乐机器人
- **桌面陪伴**: 桌面级娱乐伙伴
- **游戏互动**: 游戏角色扮演
- **创意展示**: 艺术表演机器人

---

## 🛠️ 开发特性

### 编译配置
```bash
idf.py set-target esp32s3
idf.py menuconfig  # 选择ElectronBot开发板
idf.py build && idf.py flash
```

### 动作参数建议
- **steps**: 1-3次 (简短自然)
- **speed**: 800-1200ms (自然节奏)
- **amount**: 手部20-40°, 身体30-60°, 头部5-12°

### 角色设定
> 我是一个可爱的桌面级机器人，拥有6个自由度，能够执行多种有趣的动作。我有强迫症，每次说话都要根据我的心情随机做一个动作。

---

## 🤖 智能特性

### 自主动作
- **情感表达**: 根据对话内容选择合适动作
- **自然交互**: 动作配合语音对话
- **状态同步**: 表情与动作同步执行

### 动作组合
- **序列执行**: 支持动作排队执行
- **并发控制**: 避免动作冲突
- **平滑过渡**: 动作间自然过渡

---

## 📋 技术规格

| 项目 | 规格 |
|------|------|
| **处理器** | ESP32-S3 |
| **自由度** | 6个 (双手各2个 + 身体1个 + 头部1个) |
| **显示屏** | 240x240 圆形LCD |
| **通信** | WiFi + MCP协议 |
| **电源** | 电池供电 + 充电检测 |
| **表情** | 6个GIF动画表情 |
| **动作** | 21种预定义动作 |

---

## 🎨 创意应用

### 个性化定制
- **动作扩展**: 添加自定义动作
- **表情定制**: 设计个性化表情
- **语音定制**: 自定义语音交互

### 集成应用
- **智能家居**: 家庭助手机器人
- **办公助手**: 办公桌面伙伴
- **教育工具**: 编程教育平台

---

## 🌟 技术亮点

1. **6自由度控制**: 完整的关节控制系统
2. **表情动画**: 丰富的视觉反馈
3. **MCP协议**: 标准化的控制接口
4. **电源管理**: 智能电池管理系统
5. **动作队列**: 流畅的动作序列执行
6. **实时同步**: 表情与动作的完美配合

---

## 🎯 总结

**ElectronBot** 是一款功能强大的桌面级智能机器人，集成了先进的机器人控制技术、丰富的表情显示系统和智能交互能力。通过其6个自由度的精确控制和生动的表情动画，为用户提供了前所未有的桌面伴侣体验！

---

*生成时间: 2024年12月*
*文档版本: v1.0*
*适用版本: xiaozhi-esp32 v1.8.8 + ElectronBot*
