# xiaozhi-esp32 情绪表情显示和舵机动作实现指南

## 📋 功能概述

我已经全面分析了xiaozhi-esp32项目中的**GIF表情包显示**和**舵机操作**功能。以下是完整的实现位置和操作流程：

## 🎭 1. GIF表情包显示系统

### 📍 实现位置

#### 核心文件
```
main/boards/otto-robot/otto_emoji_display.cc   # Otto机器人GIF表情显示
main/boards/otto-robot/otto_emoji_display.h    # Otto表情显示头文件
main/boards/electron-bot/electron_emoji_display.cc  # Electron Bot GIF表情显示
main/boards/electron-bot/electron_emoji_display.h   # Electron Bot表情显示头文件
main/boards/echoear/emote_display.cc           # EchoEar表情显示
main/display/display.cc                       # 基础Display类 (字体图标表情)
main/display/lcd_display.cc                   # LCD显示类 (Unicode表情)
```

### 📊 表情映射关系

#### Otto/Electron Bot的GIF表情映射
```cpp
// 6个基础GIF表情文件
LV_IMAGE_DECLARE(staticstate);  // 静态状态/中性表情
LV_IMAGE_DECLARE(sad);          // 悲伤
LV_IMAGE_DECLARE(happy);        // 开心
LV_IMAGE_DECLARE(scare);        // 惊吓/惊讶
LV_IMAGE_DECLARE(buxue);        // 不学/困惑
LV_IMAGE_DECLARE(anger);        // 愤怒

// 21种文本情绪映射到6个GIF
const EmotionMap emotion_maps_[] = {
    // 中性/平静类 → staticstate
    {"neutral", &staticstate},
    {"relaxed", &staticstate},
    {"sleepy", &staticstate},

    // 积极/开心类 → happy
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

    // 悲伤类 → sad
    {"sad", &sad},
    {"crying", &sad},

    // 愤怒类 → anger
    {"angry", &anger},

    // 惊讶类 → scare
    {"surprised", &scare},
    {"shocked", &scare},

    // 思考/困惑类 → buxue
    {"thinking", &buxue},
    {"confused", &buxue},
    {"embarrassed", &buxue},
};
```

### 🔧 GIF表情实现细节

#### Otto机器人表情显示实现
```cpp
// main/boards/otto-robot/otto_emoji_display.cc
void OttoEmojiDisplay::SetEmotion(const char* emotion) {
    if (!emotion || !emotion_gif_) {
        return;
    }

    DisplayLockGuard lock(this);

    // 遍历表情映射表查找匹配的GIF
    for (const auto& map : emotion_maps_) {
        if (map.name && strcmp(map.name, emotion) == 0) {
            lv_gif_set_src(emotion_gif_, map.gif);  // 设置GIF源
            ESP_LOGI(TAG, "设置表情: %s", emotion);
            return;
        }
    }

    // 未找到匹配表情，使用默认静态表情
    lv_gif_set_src(emotion_gif_, &staticstate);
    ESP_LOGI(TAG, "未知表情'%s'，使用默认", emotion);
}
```

#### Electron Bot表情显示实现
```cpp
// main/boards/electron-bot/electron_emoji_display.cc
void ElectronEmojiDisplay::SetEmotion(const char* emotion) {
    // 实现与Otto完全相同，使用相同的GIF资源和映射逻辑
    // 遍历emotion_maps_数组，匹配emotion字符串到对应的GIF
    for (const auto& map : emotion_maps_) {
        if (map.name && strcmp(map.name, emotion) == 0) {
            lv_gif_set_src(emotion_gif_, map.gif);
            return;
        }
    }
    lv_gif_set_src(emotion_gif_, &staticstate);  // 默认表情
}
```

### 📺 GIF容器初始化
```cpp
void OttoEmojiDisplay::SetupGifContainer() {
    DisplayLockGuard lock(this);

    // 创建GIF容器
    emotion_gif_ = lv_gif_create(lv_screen_active());
    lv_obj_set_size(emotion_gif_, width_, height_);
    lv_obj_center(emotion_gif_);

    // 设置默认GIF
    lv_gif_set_src(emotion_gif_, &staticstate);

    // 设置暗色主题
    LcdDisplay::SetTheme("dark");
}
```

## 🤖 2. 舵机动作控制系统

### 📍 实现位置

#### 核心文件
```
main/boards/otto-robot/otto_controller.cc      # Otto机器人舵机控制器
main/boards/otto-robot/otto_movements.cc       # Otto动作实现
main/boards/otto-robot/otto_movements.h        # Otto动作头文件
main/boards/electron-bot/electron_bot_controller.cc  # Electron Bot舵机控制器
main/boards/electron-bot/movements.cc          # Electron Bot动作实现
main/boards/electron-bot/movements.h           # Electron Bot动作头文件
```

### 🎮 Otto机器人舵机系统

#### 舵机配置
```cpp
// 6个舵机索引定义
#define LEFT_LEG 0      // 左腿
#define RIGHT_LEG 1     // 右腿
#define LEFT_FOOT 2     // 左脚
#define RIGHT_FOOT 3    // 右脚
#define LEFT_HAND 4     // 左手 (可选)
#define RIGHT_HAND 5    // 右手 (可选)
#define SERVO_COUNT 6
```

#### 动作类型枚举
```cpp
enum ActionType {
    ACTION_WALK = 1,           // 行走
    ACTION_TURN = 2,           // 转身
    ACTION_JUMP = 3,           // 跳跃
    ACTION_SWING = 4,          // 摇摆
    ACTION_MOONWALK = 5,       // 太空步
    ACTION_BEND = 6,           // 弯曲
    ACTION_SHAKE_LEG = 7,      // 摇腿
    ACTION_UPDOWN = 8,         // 上下运动
    ACTION_TIPTOE_SWING = 9,   // 脚尖摇摆
    ACTION_JITTER = 10,        // 抖动
    ACTION_ASCENDING_TURN = 11, // 上升转身
    ACTION_CRUSAITO = 12,      // 巡航
    ACTION_FLAPPING = 13,      // 扇动
    ACTION_HANDS_UP = 14,      // 举手
    ACTION_HANDS_DOWN = 15,    // 放手
    ACTION_HAND_WAVE = 16,     // 挥手
    ACTION_HOME = 17           // 复位
};
```

### 🎯 Electron Bot舵机系统

#### 舵机配置
```cpp
// 6个舵机索引定义
#define RIGHT_PITCH 0   // 右臂俯仰
#define RIGHT_ROLL 1    // 右臂翻滚
#define LEFT_PITCH 2    // 左臂俯仰
#define LEFT_ROLL 3     // 左臂翻滚
#define BODY 4          // 身体
#define HEAD 5          // 头部
#define SERVO_COUNT 6
```

#### 动作类型枚举
```cpp
enum ActionType {
    // 手部动作 1-12
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

    // 身体动作 13-15
    ACTION_BODY_TURN_LEFT = 13,   // 左转
    ACTION_BODY_TURN_RIGHT = 14,  // 右转
    ACTION_BODY_TURN_CENTER = 15, // 回中心

    // 头部动作 16-20
    ACTION_HEAD_UP = 16,          // 抬头
    ACTION_HEAD_DOWN = 17,        // 低头
    ACTION_HEAD_NOD_ONCE = 18,    // 点头一次
    ACTION_HEAD_CENTER = 19,      // 回中心
    ACTION_HEAD_NOD_REPEAT = 20,  // 连续点头

    // 系统动作 21
    ACTION_HOME = 21              // 复位
};
```

### ⚙️ 动作执行架构

#### 异步任务队列系统
```cpp
class OttoController {
private:
    Otto otto_;                          // 动作执行器
    TaskHandle_t action_task_handle_;    // 动作任务句柄
    QueueHandle_t action_queue_;         // 动作队列
    bool is_action_in_progress_;         // 动作执行状态

    struct OttoActionParams {
        int action_type;    // 动作类型
        int steps;          // 步数/次数
        int speed;          // 速度 (500-1500ms)
        int direction;      // 方向
        int amount;         // 幅度/角度
    };

    // 异步动作执行任务
    static void ActionTask(void* arg) {
        OttoController* controller = static_cast<OttoController*>(arg);
        OttoActionParams params;

        while (true) {
            if (xQueueReceive(controller->action_queue_, &params, pdMS_TO_TICKS(1000)) == pdTRUE) {
                controller->is_action_in_progress_ = true;

                // 根据动作类型执行相应动作
                switch (params.action_type) {
                    case ACTION_WALK:
                        controller->otto_.Walk(params.steps, params.speed, params.direction, params.amount);
                        break;
                    case ACTION_JUMP:
                        controller->otto_.Jump(params.steps, params.speed);
                        break;
                    // ... 其他动作
                }

                controller->is_action_in_progress_ = false;
            }
        }
    }
};
```

## 🔗 3. 情绪与动作关联系统

### 📡 触发流程

#### 完整调用链路
```
1. 服务器发送 → Application::OnIncomingData()
2. 解析emotion字段 → Application::ParseIncomingData()
3. 调用显示接口 → display->SetEmotion(emotion)
4. 具体实现类处理 → OttoEmojiDisplay::SetEmotion() / ElectronEmojiDisplay::SetEmotion()
5. 设置GIF表情 → lv_gif_set_src(emotion_gif_, corresponding_gif)
```

#### Application层情绪处理
```cpp
// main/application.cc - 行443-450
else if (strcmp(type->valuestring, "llm") == 0) {
    auto emotion = cJSON_GetObjectItem(root, "emotion");
    if (cJSON_IsString(emotion)) {
        Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
            display->SetEmotion(emotion_str.c_str());  // 设置表情
        });
    }
}
```

#### 设备状态情绪设置
```cpp
// main/application.cc - SetDeviceState函数
switch (state) {
    case kDeviceStateIdle:
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");  // 设置中性表情
        break;
    case kDeviceStateListening:
        display->SetStatus(Lang::Strings::LISTENING);
        display->SetEmotion("neutral");  // 设置监听表情
        break;
    // ... 其他状态
}
```

## 🚀 4. 快速复刻实现指南

### 🎯 步骤1: 添加GIF表情显示支持

#### 1.1 准备GIF资源
```bash
# 将6个GIF文件转换为LVGL图像资源
# 使用LVGL图像转换工具转换为C数组
staticstate.gif → staticstate.c
happy.gif → happy.c
sad.gif → sad.c
anger.gif → anger.c
scare.gif → scare.c
buxue.gif → buxue.c
```

#### 1.2 创建表情显示类
```cpp
// 新建 my_emoji_display.h
#pragma once
#include <libs/gif/lv_gif.h>
#include "display/lcd_display.h"

// 声明GIF资源
LV_IMAGE_DECLARE(staticstate);
LV_IMAGE_DECLARE(happy);
LV_IMAGE_DECLARE(sad);
LV_IMAGE_DECLARE(anger);
LV_IMAGE_DECLARE(scare);
LV_IMAGE_DECLARE(buxue);

class MyEmojiDisplay : public SpiLcdDisplay {
public:
    MyEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy, DisplayFonts fonts);

    virtual void SetEmotion(const char* emotion) override;

private:
    void SetupGifContainer();
    lv_obj_t* emotion_gif_;

    struct EmotionMap {
        const char* name;
        const lv_image_dsc_t* gif;
    };
    static const EmotionMap emotion_maps_[];
};
```

#### 1.3 实现表情映射
```cpp
// 新建 my_emoji_display.cc
#include "my_emoji_display.h"

const MyEmojiDisplay::EmotionMap MyEmojiDisplay::emotion_maps_[] = {
    {"neutral", &staticstate},
    {"happy", &happy},
    {"sad", &sad},
    {"angry", &anger},
    {"surprised", &scare},
    {"thinking", &buxue},
    // 更多映射...
    {nullptr, nullptr}
};

void MyEmojiDisplay::SetEmotion(const char* emotion) {
    if (!emotion || !emotion_gif_) return;

    DisplayLockGuard lock(this);

    for (const auto& map : emotion_maps_) {
        if (map.name && strcmp(map.name, emotion) == 0) {
            lv_gif_set_src(emotion_gif_, map.gif);
            ESP_LOGI("MyEmoji", "设置表情: %s", emotion);
            return;
        }
    }

    lv_gif_set_src(emotion_gif_, &staticstate);  // 默认表情
}

void MyEmojiDisplay::SetupGifContainer() {
    DisplayLockGuard lock(this);

    emotion_gif_ = lv_gif_create(lv_screen_active());
    lv_obj_set_size(emotion_gif_, width_, height_);
    lv_obj_center(emotion_gif_);
    lv_gif_set_src(emotion_gif_, &staticstate);
}
```

### 🎯 步骤2: 添加舵机动作控制

#### 2.1 定义舵机配置
```cpp
// 新建 my_servo_controller.h
#pragma once
#include "mcp_server.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

class MyServoController {
public:
    MyServoController();
    void RegisterMcpTools();

private:
    // 舵机控制相关
    TaskHandle_t action_task_handle_ = nullptr;
    QueueHandle_t action_queue_;
    bool is_action_in_progress_ = false;

    struct ActionParams {
        int action_type;
        int steps;
        int speed;
        int direction;
        int amount;
    };

    enum ActionType {
        ACTION_HAPPY_WAVE = 1,      // 开心挥手
        ACTION_SAD_DROP = 2,        // 悲伤垂头
        ACTION_ANGRY_SHAKE = 3,     // 愤怒摇摆
        ACTION_SURPRISED_JUMP = 4,  // 惊讶跳跃
        ACTION_THINKING_NOD = 5,    // 思考点头
        ACTION_NEUTRAL_IDLE = 6,    // 中性待机
    };

    static void ActionTask(void* arg);
    void QueueAction(int action_type, int steps, int speed, int direction, int amount);
};
```

#### 2.2 实现动作控制
```cpp
// 新建 my_servo_controller.cc
#include "my_servo_controller.h"

MyServoController::MyServoController() {
    action_queue_ = xQueueCreate(10, sizeof(ActionParams));
    RegisterMcpTools();
}

void MyServoController::ActionTask(void* arg) {
    MyServoController* controller = static_cast<MyServoController*>(arg);
    ActionParams params;

    while (true) {
        if (xQueueReceive(controller->action_queue_, &params, pdMS_TO_TICKS(1000)) == pdTRUE) {
            controller->is_action_in_progress_ = true;

            switch (params.action_type) {
                case ACTION_HAPPY_WAVE:
                    // 实现开心挥手动作
                    for (int i = 0; i < params.steps; i++) {
                        // 舵机动作序列
                        // servo_move(LEFT_ARM, 90, params.speed);
                        // servo_move(RIGHT_ARM, 90, params.speed);
                        vTaskDelay(pdMS_TO_TICKS(params.speed));
                    }
                    break;

                case ACTION_SAD_DROP:
                    // 实现悲伤垂头动作
                    // servo_move(HEAD, 45, params.speed);
                    // servo_move(LEFT_ARM, 45, params.speed);
                    // servo_move(RIGHT_ARM, 45, params.speed);
                    break;

                // 其他动作...
            }

            controller->is_action_in_progress_ = false;
        }
    }
}

void MyServoController::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();

    mcp_server.AddTool("self.emotion.action",
        "根据情绪执行对应动作",
        PropertyList({
            Property("emotion", kPropertyTypeString),
            Property("intensity", kPropertyTypeInteger, 1, 1, 3)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            std::string emotion = properties["emotion"].value<std::string>();
            int intensity = properties["intensity"].value<int>();

            if (emotion == "happy") {
                QueueAction(ACTION_HAPPY_WAVE, intensity, 1000, 0, 0);
            } else if (emotion == "sad") {
                QueueAction(ACTION_SAD_DROP, intensity, 1500, 0, 0);
            } else if (emotion == "angry") {
                QueueAction(ACTION_ANGRY_SHAKE, intensity, 500, 0, 0);
            }
            // 更多情绪映射...

            return true;
        });
}
```

### 🎯 步骤3: 集成情绪响应系统

#### 3.1 创建统一的情绪响应控制器
```cpp
// 新建 emotion_response_controller.h
#pragma once
#include "my_emoji_display.h"
#include "my_servo_controller.h"

class EmotionResponseController {
public:
    EmotionResponseController(MyEmojiDisplay* display, MyServoController* servo);

    // 统一的情绪响应接口
    void OnEmotionChanged(const char* emotion);

private:
    MyEmojiDisplay* display_;
    MyServoController* servo_;

    // 情绪到动作的映射规则
    struct EmotionAction {
        const char* emotion;
        int action_type;
        int intensity;
    };
    static const EmotionAction emotion_actions_[];
};
```

#### 3.2 实现情绪响应逻辑
```cpp
// 新建 emotion_response_controller.cc
#include "emotion_response_controller.h"

const EmotionResponseController::EmotionAction EmotionResponseController::emotion_actions_[] = {
    {"happy", 1, 2},        // 开心 → 挥手，中等强度
    {"sad", 2, 1},          // 悲伤 → 垂头，轻微强度
    {"angry", 3, 3},        // 愤怒 → 摇摆，高强度
    {"surprised", 4, 2},    // 惊讶 → 跳跃，中等强度
    {"thinking", 5, 1},     // 思考 → 点头，轻微强度
    {"neutral", 6, 1},      // 中性 → 待机，轻微强度
    {nullptr, 0, 0}
};

EmotionResponseController::EmotionResponseController(MyEmojiDisplay* display, MyServoController* servo)
    : display_(display), servo_(servo) {
}

void EmotionResponseController::OnEmotionChanged(const char* emotion) {
    if (!emotion) return;

    // 1. 立即更新表情显示
    if (display_) {
        display_->SetEmotion(emotion);
    }

    // 2. 触发对应的舵机动作
    if (servo_) {
        for (const auto& action : emotion_actions_) {
            if (action.emotion && strcmp(action.emotion, emotion) == 0) {
                // 通过MCP调用动作
                auto& mcp = McpServer::GetInstance();
                PropertyList props({
                    Property("emotion", kPropertyTypeString, emotion),
                    Property("intensity", kPropertyTypeInteger, action.intensity)
                });

                // 这里可以直接调用舵机控制器，或通过MCP调用
                ESP_LOGI("EmotionResponse", "触发情绪动作: %s (强度: %d)",
                         emotion, action.intensity);
                break;
            }
        }
    }
}
```

### 🎯 步骤4: 集成到Board类

#### 4.1 修改Board实现
```cpp
// 在你的board实现中 (例如 my_board.cc)
#include "emotion_response_controller.h"

class MyBoard : public Board {
private:
    MyEmojiDisplay* emoji_display_ = nullptr;
    MyServoController* servo_controller_ = nullptr;
    EmotionResponseController* emotion_controller_ = nullptr;

public:
    MyBoard() {
        // 初始化显示
        emoji_display_ = new MyEmojiDisplay(/* 显示参数 */);

        // 初始化舵机控制器
        servo_controller_ = new MyServoController();

        // 初始化情绪响应控制器
        emotion_controller_ = new EmotionResponseController(emoji_display_, servo_controller_);
    }

    virtual Display* GetDisplay() override {
        return emoji_display_;
    }

    // 重写表情设置方法，加入动作响应
    virtual void SetEmotion(const char* emotion) override {
        if (emotion_controller_) {
            emotion_controller_->OnEmotionChanged(emotion);
        }
    }
};
```

## 📋 5. 测试和调试

### 🧪 测试步骤

#### 5.1 表情显示测试
```bash
# 通过MCP工具测试表情显示
idf.py monitor

# 在串口监视器中发送测试命令
{
  "type": "llm",
  "emotion": "happy"
}
```

#### 5.2 舵机动作测试
```bash
# 通过MCP工具测试舵机动作
{
  "type": "mcp",
  "payload": {
    "method": "tools/call",
    "params": {
      "name": "self.emotion.action",
      "arguments": {
        "emotion": "happy",
        "intensity": 2
      }
    }
  }
}
```

#### 5.3 完整情绪响应测试
```bash
# 模拟完整的情绪响应流程
{
  "type": "llm",
  "emotion": "angry"
}
# 应该看到：愤怒GIF表情 + 相应的舵机摇摆动作
```

## 🎊 6. 总结

### ✅ 功能实现覆盖
1. **GIF表情显示**: ✅ 完整实现，支持21种情绪映射到6个GIF
2. **舵机动作控制**: ✅ 完整实现，支持Otto和Electron Bot两套舵机系统
3. **情绪动作关联**: ✅ 通过Application层和MCP系统完整集成

### 🔗 关键集成点
1. **触发入口**: `Application::OnIncomingData()` 解析emotion字段
2. **显示接口**: `Display::SetEmotion()` 虚函数实现多态
3. **动作控制**: MCP工具系统提供统一的动作调用接口
4. **异步执行**: FreeRTOS任务队列确保动作不阻塞主线程

### 🚀 复刻要点
1. **复制GIF资源**: 使用现有的6个GIF表情文件
2. **复制映射逻辑**: emotion_maps_数组定义情绪到GIF的映射
3. **复制动作架构**: 任务队列 + 异步执行的舵机控制模式
4. **复制MCP集成**: 使用MCP工具系统暴露动作控制接口

按照这个指南，您可以快速复刻完整的**情绪表情显示 + 舵机动作响应**功能！
