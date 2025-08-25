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

// 新增减速电机配置
// 头部上下转动电机
#define HEAD_MOTOR_PWM_PIN     GPIO_NUM_1   // 头部电机PWM
#define HEAD_MOTOR_DIR1_PIN    GPIO_NUM_2   // 头部电机方向1
#define HEAD_MOTOR_DIR2_PIN    GPIO_NUM_3   // 头部电机方向2

// 身体左右转动电机
#define BODY_MOTOR_PWM_PIN     GPIO_NUM_14  // 身体电机PWM
#define BODY_MOTOR_DIR1_PIN    GPIO_NUM_19  // 身体电机方向1
#define BODY_MOTOR_DIR2_PIN    GPIO_NUM_20  // 身体电机方向2

// 电机控制参数
#define MOTOR_PWM_FREQUENCY    1000         // PWM频率 1KHz
#define MOTOR_PWM_RESOLUTION   LEDC_TIMER_10_BIT  // 10位分辨率 (0-1023)
#define MOTOR_MAX_SPEED        800          // 最大速度 (0-1023)
#define MOTOR_MIN_SPEED        200          // 最小速度
#define MOTOR_STOP_SPEED       0            // 停止速度

// 电机角度限制
#define HEAD_MIN_ANGLE         -45          // 头部最小角度（向下）
#define HEAD_MAX_ANGLE         45           // 头部最大角度（向上）
#define BODY_MIN_ANGLE         -90          // 身体最小角度（向左）
#define BODY_MAX_ANGLE         90           // 身体最大角度（向右）

// 电机运动参数
#define MOTOR_DEFAULT_SPEED    400          // 默认运动速度
#define MOTOR_SMOOTH_SPEED     200          // 平滑运动速度
#define MOTOR_FAST_SPEED       600          // 快速运动速度

// LEDC通道分配
#define HEAD_MOTOR_PWM_CHANNEL LEDC_CHANNEL_0
#define BODY_MOTOR_PWM_CHANNEL LEDC_CHANNEL_1
#define MOTOR_PWM_TIMER        LEDC_TIMER_1

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
