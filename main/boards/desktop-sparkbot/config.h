#ifndef _DESKTOP_SPARKBOT_CONFIG_H_
#define _DESKTOP_SPARKBOT_CONFIG_H_

#include <driver/gpio.h>
#include <driver/uart.h>
#include <driver/ledc.h>

// 音频配置
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_45
#define AUDIO_I2S_GPIO_WS GPIO_NUM_41
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_39
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_40
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_42

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_46
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_4
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_5
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

// 按键配置
#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

// 显示器配置
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_DC_GPIO     GPIO_NUM_43
#define DISPLAY_CS_GPIO     GPIO_NUM_44
#define DISPLAY_CLK_GPIO    GPIO_NUM_21
#define DISPLAY_MOSI_GPIO   GPIO_NUM_47
#define DISPLAY_RST_GPIO    GPIO_NUM_NC

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_46
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// UART配置 (保留原有底盘功能)
#define UART_ECHO_TXD GPIO_NUM_38
#define UART_ECHO_RXD GPIO_NUM_48
#define UART_ECHO_RTS (-1)
#define UART_ECHO_CTS (-1)

#define ECHO_UART_PORT_NUM      UART_NUM_1
#define ECHO_UART_BAUD_RATE     (115200)
#define BUF_SIZE                (1024)

// 新增减速电机配置（简化版：每个电机2个引脚控制）
// 头部上下转动电机
#define HEAD_MOTOR_PIN1        GPIO_NUM_1   // 头部电机引脚1
#define HEAD_MOTOR_PIN2        GPIO_NUM_2   // 头部电机引脚2

// 身体左右转动电机
#define BODY_MOTOR_PIN1        GPIO_NUM_14  // 身体电机引脚1
#define BODY_MOTOR_PIN2        GPIO_NUM_3   // 身体电机引脚2

// 步进电机控制模式定义
#define MOTOR_STOP             0            // 停止: 00
#define MOTOR_FORWARD          1            // 正转: 01
#define MOTOR_BACKWARD         2            // 反转: 10

// 步进电机速度等级（根据情绪区分）
#define MOTOR_SPEED_SLOW       1            // 慢速：平静、思考、待机
#define MOTOR_SPEED_NORMAL     2            // 中速：开心、悲伤
#define MOTOR_SPEED_FAST       3            // 快速：愤怒、惊讶

// 步进电机脉冲间隔（毫秒）- 间隔越小速度越快
#define STEPPER_DELAY_SLOW     50           // 慢速：50ms间隔
#define STEPPER_DELAY_NORMAL   25           // 中速：25ms间隔
#define STEPPER_DELAY_FAST     10           // 快速：10ms间隔

// 电机角度限制
#define HEAD_MIN_ANGLE         -45          // 头部最小角度（向下）
#define HEAD_MAX_ANGLE         45           // 头部最大角度（向上）
#define BODY_MIN_ANGLE         -90          // 身体最小角度（向左）
#define BODY_MAX_ANGLE         90           // 身体最大角度（向右）

// 动作组合参数
#define IDLE_ACTION_INTERVAL   5000         // 待机动作间隔（5秒）
#define STEP_ANGLE             5            // 每步角度（度）
#define NOD_STEPS              3            // 点头步数
#define SHAKE_STEPS            4            // 摇头步数

// 底盘控制配置
#define MOTOR_SPEED_MAX 100
#define MOTOR_SPEED_80  80
#define MOTOR_SPEED_60  60
#define MOTOR_SPEED_MIN 0

typedef enum {
    LIGHT_MODE_CHARGING_BREATH = 0,
    LIGHT_MODE_POWER_LOW,
    LIGHT_MODE_ALWAYS_ON,
    LIGHT_MODE_BLINK,
    LIGHT_MODE_WHITE_BREATH_SLOW,
    LIGHT_MODE_WHITE_BREATH_FAST,
    LIGHT_MODE_FLOWING,
    LIGHT_MODE_SHOW,
    LIGHT_MODE_SLEEP,
    LIGHT_MODE_MAX
} light_mode_t;

// 摄像头配置 (保留原有功能)
#define SPARKBOT_CAMERA_XCLK      (GPIO_NUM_15)
#define SPARKBOT_CAMERA_PCLK      (GPIO_NUM_13)
#define SPARKBOT_CAMERA_VSYNC     (GPIO_NUM_6)
#define SPARKBOT_CAMERA_HSYNC     (GPIO_NUM_7)
#define SPARKBOT_CAMERA_D0        (GPIO_NUM_11)
#define SPARKBOT_CAMERA_D1        (GPIO_NUM_9)
#define SPARKBOT_CAMERA_D2        (GPIO_NUM_8)
#define SPARKBOT_CAMERA_D3        (GPIO_NUM_10)
#define SPARKBOT_CAMERA_D4        (GPIO_NUM_12)
#define SPARKBOT_CAMERA_D5        (GPIO_NUM_18)
#define SPARKBOT_CAMERA_D6        (GPIO_NUM_17)
#define SPARKBOT_CAMERA_D7        (GPIO_NUM_16)

#define SPARKBOT_CAMERA_PWDN      (GPIO_NUM_NC)
#define SPARKBOT_CAMERA_RESET     (GPIO_NUM_NC)
#define SPARKBOT_CAMERA_XCLK_FREQ (16000000)
#define SPARKBOT_LEDC_TIMER       (LEDC_TIMER_0)
#define SPARKBOT_LEDC_CHANNEL     (LEDC_CHANNEL_0)

#define SPARKBOT_CAMERA_SIOD      (GPIO_NUM_NC)
#define SPARKBOT_CAMERA_SIOC      (GPIO_NUM_NC)

#endif // _DESKTOP_SPARKBOT_CONFIG_H_
