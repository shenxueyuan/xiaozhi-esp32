#pragma once

#include "config.h"
#include "mcp_server.h"
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>

class MotorController {
public:
    MotorController();
    ~MotorController();

    // 初始化电机
    void Initialize();

    // 头部控制（步进电机版：基于步数和速度）
    void HeadUpDown(int angle, int speed = MOTOR_SPEED_NORMAL);
    void HeadUp(int steps = 1, int speed = MOTOR_SPEED_NORMAL);
    void HeadDown(int steps = 1, int speed = MOTOR_SPEED_NORMAL);
    void HeadCenter(int speed = MOTOR_SPEED_NORMAL);
    void HeadNod(int times = 1, int speed = MOTOR_SPEED_NORMAL);

    // 身体控制（步进电机版：基于步数和速度）
    void BodyLeftRight(int angle, int speed = MOTOR_SPEED_NORMAL);
    void BodyTurnLeft(int steps = 1, int speed = MOTOR_SPEED_NORMAL);
    void BodyTurnRight(int steps = 1, int speed = MOTOR_SPEED_NORMAL);
    void BodyCenter(int speed = MOTOR_SPEED_NORMAL);
    void BodyShake(int times = 1, int speed = MOTOR_SPEED_NORMAL);

    // 复合动作组合
    void PerformIdleAction();           // 日常待机动作
    void PerformEmotionAction(const std::string& emotion, int intensity = 2);

    // 复合动作
    void ExpressHappy(int intensity = 2);
    void ExpressSad(int intensity = 1);
    void ExpressAngry(int intensity = 3);
    void ExpressSurprised(int intensity = 2);
    void ExpressThinking(int intensity = 1);
    void ExpressNeutral(int intensity = 1);

    // 停止所有电机
    void StopAll();

    // 获取当前角度
    int GetHeadAngle() const { return current_head_angle_; }
    int GetBodyAngle() const { return current_body_angle_; }

    // 注册MCP工具
    void RegisterMcpTools();

private:
    // 电机控制状态
    int current_head_angle_;
    int current_body_angle_;
    bool is_moving_;

    // 任务和队列
    TaskHandle_t motor_task_handle_;
    QueueHandle_t motor_queue_;

    // 电机命令结构
    struct MotorCommand {
        enum Type {
            HEAD_MOVE,
            BODY_MOVE,
            STOP_ALL,
            COMPLEX_ACTION
        } type;

        int target_angle;
        int speed;        // 运动速度等级（1-3）
        int steps;
        int times;
    };

    // 私有方法
    void InitializeGPIO();
    void SetMotorState(bool is_head, int state);
    void StepMotor(bool is_head, int direction, int steps, int speed);
    void MoveToAngle(bool is_head, int target_angle, int speed);
    void ExecuteMotorCommand(const MotorCommand& cmd);
    int GetStepDelay(int speed);        // 根据速度等级获取脉冲延迟

    // 待机动作相关
    TaskHandle_t idle_task_handle_;
    static void IdleActionTask(void* arg);

    // 电机任务
    static void MotorTask(void* arg);

    // 队列操作
    void QueueCommand(MotorCommand::Type type, int angle = 0, int speed = MOTOR_SPEED_NORMAL,
                     int steps = 1, int times = 1);
};
