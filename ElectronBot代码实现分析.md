# ElectronBot 代码实现分析

## 1. 代码结构概览

ElectronBot的实现主要由以下几个核心文件组成：

- `electron_bot.cc`: 主板类实现，负责初始化硬件和基础组件
- `electron_bot_controller.cc`: 控制器实现，负责动作控制和MCP工具注册
- `movements.h/cc`: 动作控制逻辑实现
- `oscillator.h/cc`: 舵机控制基础类
- `electron_emoji_display.h/cc`: 表情显示实现
- `power_manager.h`: 电源管理实现

## 2. 核心类分析

### 2.1 ElectronBot 类

```cpp
class ElectronBot : public WifiBoard {
private:
    Display* display_;
    PowerManager* power_manager_;
    Button boot_button_;

    // 初始化方法
    void InitializePowerManager();
    void InitializeSpi();
    void InitializeGc9a01Display();
    void InitializeButtons();
    void InitializeController();

public:
    ElectronBot();

    // 重写WifiBoard基类方法
    virtual AudioCodec* GetAudioCodec() override;
    virtual Display* GetDisplay() override;
    virtual Backlight* GetBacklight() override;
    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override;
};
```

这个类继承自`WifiBoard`，负责初始化ElectronBot的硬件组件，包括：
- SPI总线初始化
- GC9A01圆形LCD显示屏初始化
- 按钮初始化
- 电源管理初始化
- 控制器初始化

### 2.2 ElectronBotController 类

```cpp
class ElectronBotController {
private:
    Otto electron_bot_;
    TaskHandle_t action_task_handle_ = nullptr;
    QueueHandle_t action_queue_;
    bool is_action_in_progress_ = false;

    enum ActionType {
        // 手部动作 1-12
        ACTION_HAND_LEFT_UP = 1,      // 举左手
        ACTION_HAND_RIGHT_UP = 2,     // 举右手
        // ...其他动作类型...
    };

    static void ActionTask(void* arg);
    void QueueAction(int action_type, int steps, int speed, int direction, int amount);
    void StartActionTaskIfNeeded();
    void LoadTrimsFromNVS();

public:
    ElectronBotController();
    ~ElectronBotController();
    void RegisterMcpTools();
};
```

这个类是ElectronBot的核心控制器，负责：
- 动作队列管理
- 舵机微调参数加载
- MCP工具注册
- 动作执行任务管理

### 2.3 Otto 类

```cpp
class Otto {
public:
    Otto();
    ~Otto();

    // 初始化方法
    void Init(int right_pitch, int right_roll, int left_pitch, int left_roll, int body, int head);

    // 舵机控制
    void AttachServos();
    void DetachServos();
    void SetTrims(int right_pitch, int right_roll, int left_pitch, int left_roll, int body, int head);

    // 基础运动控制
    void MoveServos(int time, int servo_target[]);
    void MoveSingle(int position, int servo_number);
    void OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                         double phase_diff[SERVO_COUNT], float cycle);

    // 预设动作
    void Home(bool hands_down = true);
    bool GetRestState();
    void SetRestState(bool state);

    // 动作分类
    void HandAction(int action, int times = 1, int amount = 30, int period = 1000);
    void BodyAction(int action, int times = 1, int amount = 30, int period = 1000);
    void HeadAction(int action, int times = 1, int amount = 10, int period = 500);

private:
    Oscillator servo_[SERVO_COUNT];
    int servo_pins_[SERVO_COUNT];
    int servo_trim_[SERVO_COUNT];
    int servo_initial_[SERVO_COUNT] = {180, 180, 0, 0, 90, 90};

    unsigned long final_time_;
    unsigned long partial_time_;
    float increment_[SERVO_COUNT];

    bool is_otto_resting_;

    void Execute(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                 double phase_diff[SERVO_COUNT], float steps);
};
```

Otto类负责ElectronBot的具体动作实现，包括：
- 舵机初始化和控制
- 舵机微调设置
- 基础运动函数
- 预设动作实现（手部、身体、头部动作）

### 2.4 Oscillator 类

```cpp
class Oscillator {
public:
    Oscillator(int trim = 0);
    ~Oscillator();

    // 舵机控制
    void Attach(int pin, bool rev = false);
    void Detach();

    // 参数设置
    void SetA(unsigned int amplitude);
    void SetO(int offset);
    void SetPh(double Ph);
    void SetT(unsigned int period);
    void SetTrim(int trim);
    void SetLimiter(int diff_limit);
    void DisableLimiter();
    int GetTrim();
    void SetPosition(int position);

    // 运动控制
    void Stop();
    void Play();
    void Reset();
    void Refresh();
    int GetPosition();

private:
    // 内部方法
    bool NextSample();
    void Write(int position);
    uint32_t AngleToCompare(int angle);

    // 舵机参数
    bool is_attached_;
    unsigned int amplitude_;
    int offset_;
    unsigned int period_;
    double phase0_;

    // 内部变量
    int pos_;
    int pin_;
    int trim_;
    double phase_;
    double inc_;
    double number_samples_;
    unsigned int sampling_period_;

    long previous_millis_;
    long current_millis_;

    bool stop_;
    bool rev_;

    int diff_limit_;
    long previous_servo_command_millis_;

    ledc_channel_t ledc_channel_;
    ledc_mode_t ledc_speed_mode_;
};
```

Oscillator类是舵机控制的基础类，提供：
- 舵机PWM控制
- 舵机振荡运动控制
- 舵机参数设置
- 舵机位置限制和速度控制

### 2.5 ElectronEmojiDisplay 类

```cpp
class ElectronEmojiDisplay : public SpiLcdDisplay {
public:
    ElectronEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height, int offset_x, int offset_y, bool mirror_x,
                         bool mirror_y, bool swap_xy);

    virtual ~ElectronEmojiDisplay() = default;

    // 重写表情设置方法
    virtual void SetEmotion(const char* emotion) override;

    // 重写聊天消息设置方法
    virtual void SetChatMessage(const char* role, const char* content) override;

private:
    void SetupGifContainer();

    lv_obj_t* emotion_gif_;  ///< GIF表情组件

    // 表情映射
    struct EmotionMap {
        const char* name;
        const lv_image_dsc_t* gif;
    };

    static const EmotionMap emotion_maps_[];
};
```

ElectronEmojiDisplay类继承自SpiLcdDisplay，负责：
- 表情GIF显示
- 表情名称到GIF的映射
- 聊天消息显示

### 2.6 PowerManager 类

```cpp
class PowerManager {
private:
    // 电池电量区间-分压电阻为2个100k
    static constexpr struct {
        uint16_t adc;
        uint8_t level;
    } BATTERY_LEVELS[];

    esp_timer_handle_t timer_handle_ = nullptr;
    gpio_num_t charging_pin_;
    adc_unit_t adc_unit_;
    adc_channel_t adc_channel_;
    uint16_t adc_values_[ADC_VALUES_COUNT];
    size_t adc_values_index_ = 0;
    size_t adc_values_count_ = 0;
    uint8_t battery_level_ = 100;
    bool is_charging_ = false;

    adc_oneshot_unit_handle_t adc_handle_;

    void CheckBatteryStatus();
    void ReadBatteryAdcData();
    void CalculateBatteryLevel(uint32_t average_adc);

public:
    PowerManager(gpio_num_t charging_pin, adc_unit_t adc_unit = ADC_UNIT_2,
                 adc_channel_t adc_channel = ADC_CHANNEL_3);
    ~PowerManager();

    void InitializeAdc();
    bool IsCharging();
    uint8_t GetBatteryLevel();
};
```

PowerManager类负责电源管理，包括：
- 电池电量监测
- 充电状态检测
- ADC初始化和读取

## 3. MCP工具注册分析

ElectronBot通过MCP协议注册了多个工具，用于控制机器人的动作。

### 3.1 手部动作工具

```cpp
mcp_server.AddTool(
    "self.electron.hand_action",
    "手部动作控制。action: 1=举手, 2=放手, 3=挥手, 4=拍打; hand: 1=左手, 2=右手, 3=双手; "
    "steps: 动作重复次数(1-10); speed: 动作速度(500-1500，数值越小越快); amount: "
    "动作幅度(10-50，仅举手动作使用)",
    PropertyList({Property("action", kPropertyTypeInteger, 1, 1, 4),
                  Property("hand", kPropertyTypeInteger, 3, 1, 3),
                  Property("steps", kPropertyTypeInteger, 1, 1, 10),
                  Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                  Property("amount", kPropertyTypeInteger, 30, 10, 50)}),
    [this](const PropertyList& properties) -> ReturnValue {
        // 实现代码
    });
```

这个工具提供了统一的手部动作控制，通过参数组合可以实现12种不同的手部动作。

### 3.2 身体动作工具

```cpp
mcp_server.AddTool(
    "self.electron.body_turn",
    "身体转向。steps: 转向步数(1-10); speed: 转向速度(500-1500，数值越小越快); direction: "
    "转向方向(1=左转, 2=右转, 3=回中心); angle: 转向角度(0-90度)",
    PropertyList({Property("steps", kPropertyTypeInteger, 1, 1, 10),
                  Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                  Property("direction", kPropertyTypeInteger, 1, 1, 3),
                  Property("angle", kPropertyTypeInteger, 45, 0, 90)}),
    [this](const PropertyList& properties) -> ReturnValue {
        // 实现代码
    });
```

这个工具提供了身体转向控制，可以控制机器人身体左转、右转和回中心。

### 3.3 头部动作工具

```cpp
mcp_server.AddTool("self.electron.head_move",
                   "头部运动。action: 1=抬头, 2=低头, 3=点头, 4=回中心, 5=连续点头; steps: "
                   "动作重复次数(1-10); speed: 动作速度(500-1500，数值越小越快); angle: "
                   "头部转动角度(1-15度)",
                   PropertyList({Property("action", kPropertyTypeInteger, 3, 1, 5),
                                 Property("steps", kPropertyTypeInteger, 1, 1, 10),
                                 Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                                 Property("angle", kPropertyTypeInteger, 5, 1, 15)}),
                   [this](const PropertyList& properties) -> ReturnValue {
                       // 实现代码
                   });
```

这个工具提供了头部动作控制，可以控制机器人头部抬头、低头、点头等动作。

### 3.4 系统工具

```cpp
mcp_server.AddTool("self.electron.stop", "立即停止", PropertyList(),
                   [this](const PropertyList& properties) -> ReturnValue {
                       // 实现代码
                   });

mcp_server.AddTool("self.electron.get_status", "获取机器人状态，返回 moving 或 idle",
                   PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                       // 实现代码
                   });
```

这些工具提供了系统级控制，如停止所有动作和获取机器人状态。

### 3.5 舵机校准工具

```cpp
mcp_server.AddTool(
    "self.electron.set_trim",
    "校准单个舵机位置。设置指定舵机的微调参数以调整ElectronBot的初始姿态，设置将永久保存。"
    "servo_type: 舵机类型(right_pitch:右臂旋转, right_roll:右臂推拉, left_pitch:左臂旋转, "
    "left_roll:左臂推拉, body:身体, head:头部); "
    "trim_value: 微调值(-30到30度)",
    PropertyList({Property("servo_type", kPropertyTypeString, "right_pitch"),
                  Property("trim_value", kPropertyTypeInteger, 0, -30, 30)}),
    [this](const PropertyList& properties) -> ReturnValue {
        // 实现代码
    });

mcp_server.AddTool("self.electron.get_trims", "获取当前的舵机微调设置", PropertyList(),
                   [this](const PropertyList& properties) -> ReturnValue {
                       // 实现代码
                   });
```

这些工具提供了舵机校准功能，可以调整和获取舵机的微调参数。

## 4. 动作实现分析

### 4.1 手部动作实现

```cpp
void Otto::HandAction(int action, int times, int amount, int period) {
    // 限制参数范围
    times = 2 * std::max(3, std::min(100, times));
    amount = std::max(10, std::min(50, amount));
    period = std::max(100, std::min(1000, period));

    int current_positions[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        current_positions[i] = (servo_pins_[i] != -1) ? servo_[i].GetPosition() : servo_initial_[i];
    }

    switch (action) {
        case 1:  // 举左手
            current_positions[LEFT_PITCH] = 180;
            MoveServos(period, current_positions);
            break;
        // ...其他手部动作实现...
    }
}
```

手部动作实现通过设置舵机目标位置并调用`MoveServos`方法实现。

### 4.2 身体动作实现

```cpp
void Otto::BodyAction(int action, int times, int amount, int period) {
    // 限制参数范围
    times = std::max(1, std::min(10, times));
    amount = std::max(0, std::min(90, amount));
    period = std::max(500, std::min(3000, period));

    int current_positions[SERVO_COUNT];
    // ...获取当前位置...

    int body_center = servo_initial_[BODY];
    int target_angle = body_center;

    switch (action) {
        case 1:  // 左转
            target_angle = body_center + amount;
            target_angle = std::min(180, target_angle);
            break;
        // ...其他身体动作实现...
    }

    current_positions[BODY] = target_angle;
    MoveServos(period, current_positions);
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

身体动作实现通过控制身体舵机的角度实现转向功能。

### 4.3 头部动作实现

```cpp
void Otto::HeadAction(int action, int times, int amount, int period) {
    // 限制参数范围
    times = std::max(1, std::min(10, times));
    amount = std::max(1, std::min(15, abs(amount)));
    period = std::max(300, std::min(3000, period));

    int current_positions[SERVO_COUNT];
    // ...获取当前位置...

    int head_center = 90;  // 头部中心位置

    switch (action) {
        case 1:  // 抬头
            current_positions[HEAD] = head_center + amount;
            MoveServos(period, current_positions);
            break;
        // ...其他头部动作实现...
    }
}
```

头部动作实现通过控制头部舵机的角度实现抬头、低头、点头等动作。

## 5. 表情显示实现

```cpp
void ElectronEmojiDisplay::SetEmotion(const char* emotion) {
    if (!emotion || !emotion_gif_) {
        return;
    }

    DisplayLockGuard lock(this);

    for (const auto& map : emotion_maps_) {
        if (map.name && strcmp(map.name, emotion) == 0) {
            lv_gif_set_src(emotion_gif_, map.gif);
            ESP_LOGI(TAG, "设置表情: %s", emotion);
            return;
        }
    }

    lv_gif_set_src(emotion_gif_, &staticstate);
    ESP_LOGI(TAG, "未知表情'%s'，使用默认", emotion);
}
```

表情显示通过映射表将表情名称映射到对应的GIF动画，然后通过LVGL库显示在LCD屏幕上。

## 6. 动作队列管理

```cpp
static void ActionTask(void* arg) {
    ElectronBotController* controller = static_cast<ElectronBotController*>(arg);
    ElectronBotActionParams params;
    controller->electron_bot_.AttachServos();

    while (true) {
        if (xQueueReceive(controller->action_queue_, &params, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI(TAG, "执行动作: %d", params.action_type);
            controller->is_action_in_progress_ = true;  // 开始执行动作

            // 执行相应的动作
            if (params.action_type >= ACTION_HAND_LEFT_UP &&
                params.action_type <= ACTION_HAND_BOTH_FLAP) {
                // 手部动作
                controller->electron_bot_.HandAction(params.action_type, params.steps,
                                                     params.amount, params.speed);
            } else if (params.action_type >= ACTION_BODY_TURN_LEFT &&
                       params.action_type <= ACTION_BODY_TURN_CENTER) {
                // 身体动作
                int body_direction = params.action_type - ACTION_BODY_TURN_LEFT + 1;
                controller->electron_bot_.BodyAction(body_direction, params.steps,
                                                     params.amount, params.speed);
            } else if (params.action_type >= ACTION_HEAD_UP &&
                       params.action_type <= ACTION_HEAD_NOD_REPEAT) {
                // 头部动作
                int head_action = params.action_type - ACTION_HEAD_UP + 1;
                controller->electron_bot_.HeadAction(head_action, params.steps, params.amount,
                                                     params.speed);
            } else if (params.action_type == ACTION_HOME) {
                // 复位动作
                controller->electron_bot_.Home(true);
            }
            controller->is_action_in_progress_ = false;  // 动作执行完毕
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

动作队列管理通过FreeRTOS队列实现，每个动作都被封装成一个参数结构体放入队列，然后由专门的任务按顺序执行。

## 7. 舵机微调管理

```cpp
void Otto::SetTrims(int right_pitch, int right_roll, int left_pitch, int left_roll, int body,
                    int head) {
    servo_trim_[RIGHT_PITCH] = right_pitch;
    servo_trim_[RIGHT_ROLL] = right_roll;
    servo_trim_[LEFT_PITCH] = left_pitch;
    servo_trim_[LEFT_ROLL] = left_roll;
    servo_trim_[BODY] = body;
    servo_trim_[HEAD] = head;

    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].SetTrim(servo_trim_[i]);
        }
    }
}
```

舵机微调通过设置每个舵机的微调值实现，这些值会被保存在NVS中，在机器人启动时加载。

## 8. 电源管理实现

```cpp
void PowerManager::CheckBatteryStatus() {
    is_charging_ = gpio_get_level(charging_pin_) == 0;
    ReadBatteryAdcData();
}

void PowerManager::ReadBatteryAdcData() {
    int adc_value;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, adc_channel_, &adc_value));

    adc_values_[adc_values_index_] = adc_value;
    adc_values_index_ = (adc_values_index_ + 1) % ADC_VALUES_COUNT;
    if (adc_values_count_ < ADC_VALUES_COUNT) {
        adc_values_count_++;
    }

    uint32_t average_adc = 0;
    for (size_t i = 0; i < adc_values_count_; i++) {
        average_adc += adc_values_[i];
    }
    average_adc /= adc_values_count_;

    CalculateBatteryLevel(average_adc);
}
```

电源管理通过定时读取ADC值和充电引脚状态，计算电池电量和充电状态。
