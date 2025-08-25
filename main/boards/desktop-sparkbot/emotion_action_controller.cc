#include "emotion_action_controller.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>

static const char* TAG = "EmotionActionController";

// 情绪动作映射规则
const EmotionActionController::EmotionActionRule EmotionActionController::emotion_action_rules_[] = {
    {"happy",       &MotorController::ExpressHappy,      2, 500},    // 开心：500ms后执行
    {"laughing",    &MotorController::ExpressHappy,      3, 300},    // 大笑：300ms后执行
    {"sad",         &MotorController::ExpressSad,        2, 800},    // 悲伤：800ms后执行
    {"crying",      &MotorController::ExpressSad,        3, 600},    // 哭泣：600ms后执行
    {"angry",       &MotorController::ExpressAngry,      3, 200},    // 愤怒：200ms后快速执行
    {"surprised",   &MotorController::ExpressSurprised,  2, 100},    // 惊讶：100ms后快速执行
    {"shocked",     &MotorController::ExpressSurprised,  3, 50},     // 震惊：50ms后立即执行
    {"thinking",    &MotorController::ExpressThinking,   1, 1000},   // 思考：1s后轻微执行
    {"confused",    &MotorController::ExpressThinking,   2, 800},    // 困惑：800ms后执行
    {"neutral",     &MotorController::ExpressNeutral,    1, 1500},   // 中性：1.5s后回中心
    {nullptr,       nullptr,                             0, 0}       // 结束标记
};

EmotionActionController::EmotionActionController(FullscreenEmojiDisplay* display, MotorController* motor)
    : display_(display)
    , motor_(motor)
    , motion_enabled_(true)
    , motion_intensity_scale_(1.0f) {

    ESP_LOGI(TAG, "情绪动作控制器初始化完成");
}

EmotionActionController::~EmotionActionController() {
    // 析构时停止所有电机
    if (motor_) {
        motor_->StopAll();
    }
}

void EmotionActionController::OnEmotionChanged(const char* emotion, int intensity) {
    if (!emotion) {
        ESP_LOGW(TAG, "收到空的情绪字符串");
        return;
    }

    ESP_LOGI(TAG, "处理情绪变化: %s (强度: %d)", emotion, intensity);

    // 1. 立即更新显示表情
    if (display_) {
        display_->SetEmotionIntensity(intensity);
        display_->SetEmotion(emotion);
    }

    // 2. 如果启用了动作响应，执行相应的电机动作
    if (motion_enabled_ && motor_) {
        for (const auto& rule : emotion_action_rules_) {
            if (rule.emotion && strcmp(rule.emotion, emotion) == 0) {
                // 计算最终强度
                int final_intensity = std::max(1, std::min(3,
                    (int)(rule.base_intensity * intensity * motion_intensity_scale_)));

                if (rule.delay_ms > 0) {
                    // 创建延迟执行任务
                    DelayedAction* action = new DelayedAction{
                        this,
                        rule.action_func,
                        final_intensity
                    };

                    // 创建延迟任务
                    xTaskCreate([](void* arg) {
                        DelayedAction* delayed_action = static_cast<DelayedAction*>(arg);
                        vTaskDelay(pdMS_TO_TICKS(500)); // 固定延迟，也可以从rule中获取

                        if (delayed_action->controller->motion_enabled_ && delayed_action->controller->motor_) {
                            (delayed_action->controller->motor_->*(delayed_action->action_func))(delayed_action->intensity);
                        }

                        delete delayed_action;
                        vTaskDelete(nullptr);
                    }, "emotion_action", 2048, action, 3, nullptr);
                } else {
                    // 立即执行
                    (motor_->*rule.action_func)(final_intensity);
                }

                ESP_LOGI(TAG, "情绪动作已触发: %s (最终强度: %d, 延迟: %dms)",
                         emotion, final_intensity, rule.delay_ms);
                break;
            }
        }
    }
}
