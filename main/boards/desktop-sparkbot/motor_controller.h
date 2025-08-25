#pragma once

#include "config.h"
#include "mcp_server.h"
#include <driver/ledc.h>
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

    // 头部控制
    void HeadUpDown(int angle, int speed = MOTOR_DEFAULT_SPEED);
    void HeadUp(int steps = 1, int speed = MOTOR_DEFAULT_SPEED);
    void HeadDown(int steps = 1, int speed = MOTOR_DEFAULT_SPEED);
    void HeadCenter(int speed = MOTOR_DEFAULT_SPEED);
    void HeadNod(int times = 1, int speed = MOTOR_DEFAULT_SPEED);

    // 身体控制
    void BodyLeftRight(int angle, int speed = MOTOR_DEFAULT_SPEED);
    void BodyTurnLeft(int steps = 1, int speed = MOTOR_DEFAULT_SPEED);
    void BodyTurnRight(int steps = 1, int speed = MOTOR_DEFAULT_SPEED);
    void BodyCenter(int speed = MOTOR_DEFAULT_SPEED);
    void BodyShake(int times = 1, int speed = MOTOR_DEFAULT_SPEED);

    // 复合动作
    void ExpressHappy(int intensity = 2);
    void ExpressSad(int intensity = 1);
    void ExpressAngry(int intensity = 3);
    void ExpressSurprised(int intensity = 2);
    void ExpressThinking(int intensity = 1);
    void ExpressNeutral();

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
        int speed;
        int steps;
        int times;
    };

    // 私有方法
    void InitializePWM();
    void InitializeGPIO();
    void SetMotorPWM(ledc_channel_t channel, int speed);
    void SetMotorDirection(gpio_num_t dir1_pin, gpio_num_t dir2_pin, bool forward);
    void MoveToAngle(bool is_head, int target_angle, int speed);
    void ExecuteMotorCommand(const MotorCommand& cmd);

    // 电机任务
    static void MotorTask(void* arg);

    // 队列操作
    void QueueCommand(MotorCommand::Type type, int angle = 0, int speed = MOTOR_DEFAULT_SPEED,
                     int steps = 1, int times = 1);
};
