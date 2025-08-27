#include "motor_controller.h"
#include <algorithm>

static const char* TAG = "MotorController";

MotorController::MotorController()
    : current_head_angle_(0)
    , current_body_angle_(0)
    , is_moving_(false)
    , motor_task_handle_(nullptr)
    , idle_task_handle_(nullptr) {
}

MotorController::~MotorController() {
    StopAll();
    if (motor_task_handle_) {
        vTaskDelete(motor_task_handle_);
    }
    if (idle_task_handle_) {
        vTaskDelete(idle_task_handle_);
    }
    if (motor_queue_) {
        vQueueDelete(motor_queue_);
    }
}

void MotorController::Initialize() {
    ESP_LOGI(TAG, "初始化电机控制器");

    InitializeGPIO();

    // 创建电机控制队列
    motor_queue_ = xQueueCreate(10, sizeof(MotorCommand));
    if (!motor_queue_) {
        ESP_LOGE(TAG, "创建电机队列失败");
        return;
    }

    // 创建电机控制任务
    xTaskCreate(MotorTask, "motor_task", 4096, this, 5, &motor_task_handle_);

    // 创建待机动作任务
    xTaskCreate(IdleActionTask, "idle_action_task", 2048, this, 3, &idle_task_handle_);

    // 注册MCP工具
    RegisterMcpTools();

    ESP_LOGI(TAG, "电机控制器初始化完成");
}



void MotorController::InitializeGPIO() {
    // 配置电机控制引脚
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << HEAD_MOTOR_PIN1) | (1ULL << HEAD_MOTOR_PIN2) |
                       (1ULL << BODY_MOTOR_PIN1) | (1ULL << BODY_MOTOR_PIN2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // 初始状态：所有电机停止 (00)
    gpio_set_level(HEAD_MOTOR_PIN1, 0);
    gpio_set_level(HEAD_MOTOR_PIN2, 0);
    gpio_set_level(BODY_MOTOR_PIN1, 0);
    gpio_set_level(BODY_MOTOR_PIN2, 0);

    ESP_LOGI(TAG, "GPIO初始化完成 - 头部电机: GPIO%d,%d 身体电机: GPIO%d,%d",
             HEAD_MOTOR_PIN1, HEAD_MOTOR_PIN2, BODY_MOTOR_PIN1, BODY_MOTOR_PIN2);
}

void MotorController::HeadUpDown(int angle, int speed) {
    angle = std::max(HEAD_MIN_ANGLE, std::min(HEAD_MAX_ANGLE, angle));
    QueueCommand(MotorCommand::HEAD_MOVE, angle, speed);
}

void MotorController::HeadUp(int steps, int speed) {
    int target_angle = std::min(HEAD_MAX_ANGLE, current_head_angle_ + steps * STEP_ANGLE);
    HeadUpDown(target_angle, speed);
}

void MotorController::HeadDown(int steps, int speed) {
    int target_angle = std::max(HEAD_MIN_ANGLE, current_head_angle_ - steps * STEP_ANGLE);
    HeadUpDown(target_angle, speed);
}

void MotorController::HeadCenter(int speed) {
    HeadUpDown(0, speed);
}

void MotorController::HeadNod(int times, int speed) {
    QueueCommand(MotorCommand::COMPLEX_ACTION, 0, speed, 0, times);
}

void MotorController::BodyLeftRight(int angle, int speed) {
    angle = std::max(BODY_MIN_ANGLE, std::min(BODY_MAX_ANGLE, angle));
    QueueCommand(MotorCommand::BODY_MOVE, angle, speed);
}

void MotorController::BodyTurnLeft(int steps, int speed) {
    int target_angle = std::max(BODY_MIN_ANGLE, current_body_angle_ - steps * STEP_ANGLE * 2);
    BodyLeftRight(target_angle, speed);
}

void MotorController::BodyTurnRight(int steps, int speed) {
    int target_angle = std::min(BODY_MAX_ANGLE, current_body_angle_ + steps * STEP_ANGLE * 2);
    BodyLeftRight(target_angle, speed);
}

void MotorController::BodyCenter(int speed) {
    BodyLeftRight(0, speed);
}

void MotorController::BodyShake(int times, int speed) {
    for (int i = 0; i < times; i++) {
        QueueCommand(MotorCommand::BODY_MOVE, -20, speed);
        QueueCommand(MotorCommand::BODY_MOVE, 20, speed);
    }
    QueueCommand(MotorCommand::BODY_MOVE, 0, speed);
}

// 情绪表达动作
void MotorController::ExpressHappy(int intensity) {
    ESP_LOGI(TAG, "执行开心表情动作，强度: %d", intensity);
    // 快速点头 + 身体欢快摇摆
    HeadNod(2, MOTOR_SPEED_NORMAL);
    BodyShake(intensity, MOTOR_SPEED_NORMAL);
    HeadUp(1, MOTOR_SPEED_NORMAL);  // 最后轻微上扬表示开心
}

void MotorController::ExpressSad(int intensity) {
    ESP_LOGI(TAG, "执行悲伤表情动作，强度: %d", intensity);
    // 缓慢下垂 + 身体微微左倾
    HeadDown(intensity + 1, MOTOR_SPEED_SLOW);
    BodyTurnLeft(1, MOTOR_SPEED_SLOW);  // 表示沉闷
}

void MotorController::ExpressAngry(int intensity) {
    ESP_LOGI(TAG, "执行愤怒表情动作，强度: %d", intensity);
    // 激烈摇头 + 身体快速摇摆
    for (int i = 0; i < intensity; i++) {
        HeadNod(2, MOTOR_SPEED_FAST);
        BodyShake(2, MOTOR_SPEED_FAST);
    }
}

void MotorController::ExpressSurprised(int intensity) {
    ESP_LOGI(TAG, "执行惊讶表情动作，强度: %d", intensity);
    // 突然停止 + 快速上扬 + 身体后缩
    StopAll();
    vTaskDelay(pdMS_TO_TICKS(100));  // 短暂停頓
    HeadUp(intensity + 1, MOTOR_SPEED_FAST);
    BodyTurnRight(1, MOTOR_SPEED_FAST);  // 身体后缩
}

void MotorController::ExpressThinking(int intensity) {
    ESP_LOGI(TAG, "执行思考表情动作，强度: %d", intensity);
    // 缓慢左右摇头 + 微微点头
    BodyTurnLeft(1, MOTOR_SPEED_SLOW);
    vTaskDelay(pdMS_TO_TICKS(500));
    BodyTurnRight(1, MOTOR_SPEED_SLOW);
    HeadNod(1, MOTOR_SPEED_SLOW);  // 轻微点头表示思考
}

void MotorController::ExpressNeutral(int intensity) {
    ESP_LOGI(TAG, "执行中性表情动作，强度: %d", intensity);
    // 平稳回到中心位置
    int speed = intensity <= 1 ? MOTOR_SPEED_SLOW : MOTOR_SPEED_NORMAL;
    HeadCenter(speed);
    BodyCenter(speed);
}

void MotorController::StopAll() {
    QueueCommand(MotorCommand::STOP_ALL);
}

// 新增：情绪驱动动作组合
void MotorController::PerformEmotionAction(const std::string& emotion, int intensity) {
    ESP_LOGI(TAG, "执行情绪动作: %s, 强度: %d", emotion.c_str(), intensity);

    if (emotion == "happy" || emotion == "laughing" || emotion == "funny") {
        ExpressHappy(intensity);
    } else if (emotion == "sad" || emotion == "crying") {
        ExpressSad(intensity);
    } else if (emotion == "angry") {
        ExpressAngry(intensity);
    } else if (emotion == "surprised" || emotion == "shocked") {
        ExpressSurprised(intensity);
    } else if (emotion == "thinking" || emotion == "confused") {
        ExpressThinking(intensity);
    } else if (emotion == "neutral" || emotion == "relaxed") {
        ExpressNeutral(intensity);
    } else {
        // 默认动作：微微点头
        HeadNod(1, MOTOR_SPEED_SLOW);
    }
}

// 新增：日常待机动作
void MotorController::PerformIdleAction() {
    static int idle_count = 0;
    idle_count++;

    ESP_LOGD(TAG, "执行待机动作 #%d", idle_count);

    // 随机选择待机动作
    int action_type = idle_count % 4;

    switch (action_type) {
        case 0:  // 微微点头，模拟观察
            HeadNod(1, MOTOR_SPEED_SLOW);
            break;

        case 1:  // 轻微左转身体，扫视
            BodyTurnLeft(1, MOTOR_SPEED_SLOW);
            vTaskDelay(pdMS_TO_TICKS(1000));
            BodyCenter(MOTOR_SPEED_SLOW);
            break;

        case 2:  // 轻微右转身体，扫视
            BodyTurnRight(1, MOTOR_SPEED_SLOW);
            vTaskDelay(pdMS_TO_TICKS(1000));
            BodyCenter(MOTOR_SPEED_SLOW);
            break;

        case 3:  // 头部上下观察
            HeadUp(1, MOTOR_SPEED_SLOW);
            vTaskDelay(pdMS_TO_TICKS(800));
            HeadDown(1, MOTOR_SPEED_SLOW);
            vTaskDelay(pdMS_TO_TICKS(800));
            HeadCenter(MOTOR_SPEED_SLOW);
            break;
    }
}

void MotorController::MotorTask(void* arg) {
    MotorController* controller = static_cast<MotorController*>(arg);
    MotorCommand cmd;

    while (true) {
        if (xQueueReceive(controller->motor_queue_, &cmd, pdMS_TO_TICKS(1000)) == pdTRUE) {
            controller->ExecuteMotorCommand(cmd);
        }
    }
}

void MotorController::ExecuteMotorCommand(const MotorCommand& cmd) {
    is_moving_ = true;

    switch (cmd.type) {
        case MotorCommand::HEAD_MOVE:
            MoveToAngle(true, cmd.target_angle, cmd.speed);
            current_head_angle_ = cmd.target_angle;
            break;

        case MotorCommand::BODY_MOVE:
            MoveToAngle(false, cmd.target_angle, cmd.speed);
            current_body_angle_ = cmd.target_angle;
            break;

        case MotorCommand::STOP_ALL:
            SetMotorState(true, MOTOR_STOP);   // 停止头部电机
            SetMotorState(false, MOTOR_STOP);  // 停止身体电机
            break;

        case MotorCommand::COMPLEX_ACTION:
            // 执行复杂动作（如点头）
            for (int i = 0; i < cmd.times; i++) {
                MoveToAngle(true, 15, cmd.speed);  // 向上
                vTaskDelay(pdMS_TO_TICKS(200));
                MoveToAngle(true, -15, cmd.speed); // 向下
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            MoveToAngle(true, 0, cmd.speed);  // 回中
            current_head_angle_ = 0;
            break;
    }

    is_moving_ = false;
}

// 待机动作任务
void MotorController::IdleActionTask(void* arg) {
    MotorController* controller = static_cast<MotorController*>(arg);

    while (true) {
        // 每隔一段时间执行待机动作
        vTaskDelay(pdMS_TO_TICKS(IDLE_ACTION_INTERVAL));

        // 只有在不运动时才执行待机动作
        if (!controller->is_moving_) {
            controller->PerformIdleAction();
        }
    }
}

void MotorController::MoveToAngle(bool is_head, int target_angle, int speed) {
    int current_angle = is_head ? current_head_angle_ : current_body_angle_;

    if (target_angle == current_angle) {
        ESP_LOGD(TAG, "%s电机已在目标位置: %d°", is_head ? "头部" : "身体", target_angle);
        return;
    }

    // 计算需要的步数
    int angle_diff = abs(target_angle - current_angle);
    int steps = angle_diff / STEP_ANGLE;
    int direction = target_angle > current_angle ? MOTOR_FORWARD : MOTOR_BACKWARD;

    ESP_LOGI(TAG, "%s电机运动: %d° -> %d°, 步数: %d, 速度: %d",
             is_head ? "头部" : "身体", current_angle, target_angle, steps, speed);

    // 执行步进运动
    StepMotor(is_head, direction, steps, speed);
}

// 新增：步进电机步进控制
void MotorController::StepMotor(bool is_head, int direction, int steps, int speed) {
    int step_delay = GetStepDelay(speed);

    ESP_LOGD(TAG, "%s电机步进: 方向=%d, 步数=%d, 延迟=%dms",
             is_head ? "头部" : "身体", direction, steps, step_delay);

    for (int i = 0; i < steps; i++) {
        // 步进脉冲：启动 -> 延迟 -> 停止 -> 延迟
        SetMotorState(is_head, direction);
        vTaskDelay(pdMS_TO_TICKS(step_delay / 2));

        SetMotorState(is_head, MOTOR_STOP);
        vTaskDelay(pdMS_TO_TICKS(step_delay / 2));
    }
}

// 新增：根据速度等级获取步进延迟
int MotorController::GetStepDelay(int speed) {
    switch (speed) {
        case MOTOR_SPEED_SLOW:   return STEPPER_DELAY_SLOW;
        case MOTOR_SPEED_NORMAL: return STEPPER_DELAY_NORMAL;
        case MOTOR_SPEED_FAST:   return STEPPER_DELAY_FAST;
        default:                 return STEPPER_DELAY_NORMAL;
    }
}

void MotorController::SetMotorState(bool is_head, int state) {
    gpio_num_t pin1, pin2;

    if (is_head) {
        pin1 = HEAD_MOTOR_PIN1;
        pin2 = HEAD_MOTOR_PIN2;
    } else {
        pin1 = BODY_MOTOR_PIN1;
        pin2 = BODY_MOTOR_PIN2;
    }

    switch (state) {
        case MOTOR_STOP:      // 00 - 停止
            gpio_set_level(pin1, 0);
            gpio_set_level(pin2, 0);
            break;

        case MOTOR_FORWARD:   // 01 - 正转
            gpio_set_level(pin1, 0);
            gpio_set_level(pin2, 1);
            break;

        case MOTOR_BACKWARD:  // 10 - 反转
            gpio_set_level(pin1, 1);
            gpio_set_level(pin2, 0);
            break;

        default:
            ESP_LOGW(TAG, "未知电机状态: %d", state);
            gpio_set_level(pin1, 0);
            gpio_set_level(pin2, 0);
            break;
    }

    ESP_LOGD(TAG, "%s电机状态: GPIO%d=%d, GPIO%d=%d",
             is_head ? "头部" : "身体",
             pin1, gpio_get_level(pin1), pin2, gpio_get_level(pin2));
}

void MotorController::QueueCommand(MotorCommand::Type type, int angle, int speed, int steps, int times) {
    MotorCommand cmd = {type, angle, speed, steps, times};
    xQueueSend(motor_queue_, &cmd, pdMS_TO_TICKS(100));
}

void MotorController::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();

    // 头部控制工具
    mcp_server.AddTool("self.head.up_down", "头部上下转动",
        PropertyList({
            Property("angle", kPropertyTypeInteger, HEAD_MIN_ANGLE, HEAD_MAX_ANGLE),
            Property("speed", kPropertyTypeInteger, MOTOR_SPEED_SLOW, MOTOR_SPEED_FAST, MOTOR_SPEED_NORMAL)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            int angle = properties["angle"].value<int>();
            int speed = properties["speed"].value<int>();
            HeadUpDown(angle, speed);
            return true;
        });

    // 身体控制工具
    mcp_server.AddTool("self.body.left_right", "身体左右转动",
        PropertyList({
            Property("angle", kPropertyTypeInteger, BODY_MIN_ANGLE, BODY_MAX_ANGLE),
            Property("speed", kPropertyTypeInteger, MOTOR_SPEED_SLOW, MOTOR_SPEED_FAST, MOTOR_SPEED_NORMAL)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            int angle = properties["angle"].value<int>();
            int speed = properties["speed"].value<int>();
            BodyLeftRight(angle, speed);
            return true;
        });

    // 情绪动作工具（升级版）
    mcp_server.AddTool("self.emotion.express", "根据情绪执行动作组合",
        PropertyList({
            Property("emotion", kPropertyTypeString),
            Property("intensity", kPropertyTypeInteger, 1, 3, 2)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            std::string emotion = properties["emotion"].value<std::string>();
            int intensity = properties["intensity"].value<int>();
            PerformEmotionAction(emotion, intensity);
            return true;
        });

    // 待机动作工具
    mcp_server.AddTool("self.action.idle", "执行日常待机动作",
        PropertyList({}),
        [this](const PropertyList& properties) -> ReturnValue {
            PerformIdleAction();
            return true;
        });
}
