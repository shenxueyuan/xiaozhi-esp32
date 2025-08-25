#include "motor_controller.h"
#include <algorithm>

static const char* TAG = "MotorController";

MotorController::MotorController()
    : current_head_angle_(0)
    , current_body_angle_(0)
    , is_moving_(false)
    , motor_task_handle_(nullptr) {
}

MotorController::~MotorController() {
    StopAll();
    if (motor_task_handle_) {
        vTaskDelete(motor_task_handle_);
    }
    if (motor_queue_) {
        vQueueDelete(motor_queue_);
    }
}

void MotorController::Initialize() {
    ESP_LOGI(TAG, "初始化电机控制器");

    InitializePWM();
    InitializeGPIO();

    // 创建电机控制队列
    motor_queue_ = xQueueCreate(10, sizeof(MotorCommand));
    if (!motor_queue_) {
        ESP_LOGE(TAG, "创建电机队列失败");
        return;
    }

    // 创建电机控制任务
    xTaskCreate(MotorTask, "motor_task", 4096, this, 5, &motor_task_handle_);

    // 注册MCP工具
    RegisterMcpTools();

    ESP_LOGI(TAG, "电机控制器初始化完成");
}

void MotorController::InitializePWM() {
    // 配置LEDC定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = MOTOR_PWM_TIMER,
        .duty_resolution = MOTOR_PWM_RESOLUTION,
        .freq_hz = MOTOR_PWM_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 配置头部电机PWM通道
    ledc_channel_config_t head_channel = {
        .gpio_num = HEAD_MOTOR_PWM_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = HEAD_MOTOR_PWM_CHANNEL,
        .timer_sel = MOTOR_PWM_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&head_channel));

    // 配置身体电机PWM通道
    ledc_channel_config_t body_channel = {
        .gpio_num = BODY_MOTOR_PWM_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BODY_MOTOR_PWM_CHANNEL,
        .timer_sel = MOTOR_PWM_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&body_channel));
}

void MotorController::InitializeGPIO() {
    // 配置电机方向控制引脚
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << HEAD_MOTOR_DIR1_PIN) | (1ULL << HEAD_MOTOR_DIR2_PIN) |
                       (1ULL << BODY_MOTOR_DIR1_PIN) | (1ULL << BODY_MOTOR_DIR2_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // 初始状态停止
    gpio_set_level(HEAD_MOTOR_DIR1_PIN, 0);
    gpio_set_level(HEAD_MOTOR_DIR2_PIN, 0);
    gpio_set_level(BODY_MOTOR_DIR1_PIN, 0);
    gpio_set_level(BODY_MOTOR_DIR2_PIN, 0);
}

void MotorController::HeadUpDown(int angle, int speed) {
    angle = std::max(HEAD_MIN_ANGLE, std::min(HEAD_MAX_ANGLE, angle));
    QueueCommand(MotorCommand::HEAD_MOVE, angle, speed);
}

void MotorController::HeadUp(int steps, int speed) {
    int target_angle = std::min(HEAD_MAX_ANGLE, current_head_angle_ + steps * 15);
    HeadUpDown(target_angle, speed);
}

void MotorController::HeadDown(int steps, int speed) {
    int target_angle = std::max(HEAD_MIN_ANGLE, current_head_angle_ - steps * 15);
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
    int target_angle = std::max(BODY_MIN_ANGLE, current_body_angle_ - steps * 30);
    BodyLeftRight(target_angle, speed);
}

void MotorController::BodyTurnRight(int steps, int speed) {
    int target_angle = std::min(BODY_MAX_ANGLE, current_body_angle_ + steps * 30);
    BodyLeftRight(target_angle, speed);
}

void MotorController::BodyCenter(int speed) {
    BodyLeftRight(0, speed);
}

void MotorController::BodyShake(int times, int speed) {
    for (int i = 0; i < times; i++) {
        QueueCommand(MotorCommand::BODY_MOVE, -30, speed);
        QueueCommand(MotorCommand::BODY_MOVE, 30, speed);
    }
    QueueCommand(MotorCommand::BODY_MOVE, 0, speed);
}

// 情绪表达动作
void MotorController::ExpressHappy(int intensity) {
    ESP_LOGI(TAG, "执行开心表情动作，强度: %d", intensity);
    // 头部轻微上扬 + 身体左右摇摆
    HeadUp(1, MOTOR_SMOOTH_SPEED);
    BodyShake(intensity, MOTOR_SMOOTH_SPEED);
}

void MotorController::ExpressSad(int intensity) {
    ESP_LOGI(TAG, "执行悲伤表情动作，强度: %d", intensity);
    // 头部下垂
    HeadDown(intensity, MOTOR_SMOOTH_SPEED);
}

void MotorController::ExpressAngry(int intensity) {
    ESP_LOGI(TAG, "执行愤怒表情动作，强度: %d", intensity);
    // 头部快速上下摇动 + 身体快速左右摇摆
    HeadNod(intensity, MOTOR_FAST_SPEED);
    BodyShake(intensity, MOTOR_FAST_SPEED);
}

void MotorController::ExpressSurprised(int intensity) {
    ESP_LOGI(TAG, "执行惊讶表情动作，强度: %d", intensity);
    // 头部快速上扬
    HeadUp(intensity, MOTOR_FAST_SPEED);
    vTaskDelay(pdMS_TO_TICKS(500));
    HeadCenter(MOTOR_SMOOTH_SPEED);
}

void MotorController::ExpressThinking(int intensity) {
    ESP_LOGI(TAG, "执行思考表情动作，强度: %d", intensity);
    // 头部轻微点头
    HeadNod(intensity, MOTOR_SMOOTH_SPEED);
}

void MotorController::ExpressNeutral() {
    ESP_LOGI(TAG, "执行中性表情动作");
    // 回到中心位置
    HeadCenter(MOTOR_SMOOTH_SPEED);
    BodyCenter(MOTOR_SMOOTH_SPEED);
}

void MotorController::StopAll() {
    QueueCommand(MotorCommand::STOP_ALL);
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
            SetMotorPWM(HEAD_MOTOR_PWM_CHANNEL, 0);
            SetMotorPWM(BODY_MOTOR_PWM_CHANNEL, 0);
            break;

        case MotorCommand::COMPLEX_ACTION:
            // 执行复杂动作（如点头）
            for (int i = 0; i < cmd.times; i++) {
                MoveToAngle(true, 20, cmd.speed);
                vTaskDelay(pdMS_TO_TICKS(300));
                MoveToAngle(true, -10, cmd.speed);
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            MoveToAngle(true, 0, cmd.speed);
            current_head_angle_ = 0;
            break;
    }

    is_moving_ = false;
}

void MotorController::MoveToAngle(bool is_head, int target_angle, int speed) {
    ledc_channel_t channel = is_head ? HEAD_MOTOR_PWM_CHANNEL : BODY_MOTOR_PWM_CHANNEL;
    gpio_num_t dir1_pin = is_head ? HEAD_MOTOR_DIR1_PIN : BODY_MOTOR_DIR1_PIN;
    gpio_num_t dir2_pin = is_head ? HEAD_MOTOR_DIR2_PIN : BODY_MOTOR_DIR2_PIN;

    int current_angle = is_head ? current_head_angle_ : current_body_angle_;
    bool forward = target_angle > current_angle;

    // 设置方向
    SetMotorDirection(dir1_pin, dir2_pin, forward);

    // 启动电机
    SetMotorPWM(channel, speed);

    // 计算运动时间（简化处理，实际应该使用编码器反馈）
    int angle_diff = abs(target_angle - current_angle);
    int move_time = angle_diff * 20; // 每度20ms

    vTaskDelay(pdMS_TO_TICKS(move_time));

    // 停止电机
    SetMotorPWM(channel, 0);
}

void MotorController::SetMotorPWM(ledc_channel_t channel, int speed) {
    speed = std::max(0, std::min(MOTOR_MAX_SPEED, speed));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, speed));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel));
}

void MotorController::SetMotorDirection(gpio_num_t dir1_pin, gpio_num_t dir2_pin, bool forward) {
    if (forward) {
        gpio_set_level(dir1_pin, 1);
        gpio_set_level(dir2_pin, 0);
    } else {
        gpio_set_level(dir1_pin, 0);
        gpio_set_level(dir2_pin, 1);
    }
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
            Property("speed", kPropertyTypeInteger, MOTOR_MIN_SPEED, MOTOR_MAX_SPEED, MOTOR_DEFAULT_SPEED)
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
            Property("speed", kPropertyTypeInteger, MOTOR_MIN_SPEED, MOTOR_MAX_SPEED, MOTOR_DEFAULT_SPEED)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            int angle = properties["angle"].value<int>();
            int speed = properties["speed"].value<int>();
            BodyLeftRight(angle, speed);
            return true;
        });

    // 情绪动作工具
    mcp_server.AddTool("self.emotion.express", "根据情绪执行动作",
        PropertyList({
            Property("emotion", kPropertyTypeString),
            Property("intensity", kPropertyTypeInteger, 1, 3, 2)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            std::string emotion = properties["emotion"].value<std::string>();
            int intensity = properties["intensity"].value<int>();

            if (emotion == "happy") {
                ExpressHappy(intensity);
            } else if (emotion == "sad") {
                ExpressSad(intensity);
            } else if (emotion == "angry") {
                ExpressAngry(intensity);
            } else if (emotion == "surprised") {
                ExpressSurprised(intensity);
            } else if (emotion == "thinking") {
                ExpressThinking(intensity);
            } else if (emotion == "neutral") {
                ExpressNeutral();
            }

            return true;
        });
}
