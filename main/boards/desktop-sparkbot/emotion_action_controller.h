#pragma once

#include "fullscreen_emoji_display.h"
#include "motor_controller.h"
#include <string>

class EmotionActionController {
public:
    EmotionActionController(FullscreenEmojiDisplay* display, MotorController* motor);
    ~EmotionActionController();

    // 统一的情绪响应接口
    void OnEmotionChanged(const char* emotion, int intensity = 2);

    // 设置是否启用动作响应
    void SetMotionEnabled(bool enabled) { motion_enabled_ = enabled; }
    bool IsMotionEnabled() const { return motion_enabled_; }

    // 设置动作强度系数
    void SetMotionIntensityScale(float scale) { motion_intensity_scale_ = scale; }

private:
    FullscreenEmojiDisplay* display_;
    MotorController* motor_;
    bool motion_enabled_;
    float motion_intensity_scale_;

    // 情绪到动作的映射规则
    struct EmotionActionRule {
        const char* emotion;
        void (MotorController::*action_func)(int);
        int base_intensity;
        int delay_ms;  // 表情显示后延迟多久执行动作
    };
    static const EmotionActionRule emotion_action_rules_[];

    // 执行延迟动作的任务
    struct DelayedAction {
        EmotionActionController* controller;
        void (MotorController::*action_func)(int);
        int intensity;
    };
    static void DelayedActionTask(void* arg);
};
