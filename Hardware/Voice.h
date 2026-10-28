#ifndef __VOICE_H
#define __VOICE_H

#include "stm32f10x.h"

/*
 * SU03T 离线语音识别模块（UART5，9600 8N1）
 *   模块 TX -> PD2 (UART5_RX)
 *   模块 RX -> PC12 (UART5_TX)
 *
 * 该模块已经烧好固件，识别到唤醒词后把识别结果按"一个字节一条命令"
 * 发出来，字节含义见下面的宏定义（与实训资料里的指令表一致）：
 *   0x10 问温度   0x11 问湿度   0x12 问光照   0x13 问烟雾
 *   0x01 开灯     0x02 关灯
 *   0x03 红色     0x04 绿色     0x05 蓝色
 *   0x06 开继电器 0x07 关继电器
 *   0x08 开蜂鸣器 0x09 关蜂鸣器
 *
 * 接收用中断 + 小环形缓冲，主循环里 Voice_Poll() 取命令，
 * 这样不会像阻塞式等待那样把屏幕和按键的任务卡住
 */

/* 命令字节 */
#define VOICE_CODE_TEM       0x10U
#define VOICE_CODE_HUM       0x11U
#define VOICE_CODE_LIGHT     0x12U
#define VOICE_CODE_SMOKE     0x13U
#define VOICE_CODE_LED_ON    0x01U
#define VOICE_CODE_LED_OFF   0x02U
#define VOICE_CODE_RED       0x03U
#define VOICE_CODE_GREEN     0x04U
#define VOICE_CODE_BLUE      0x05U
#define VOICE_CODE_RELAY_ON  0x06U
#define VOICE_CODE_RELAY_OFF 0x07U
#define VOICE_CODE_BEEP_ON   0x08U
#define VOICE_CODE_BEEP_OFF  0x09U

typedef enum
{
    VOICE_NONE = 0,
    VOICE_TEM,
    VOICE_HUM,
    VOICE_LIGHT,
    VOICE_SMOKE,
    VOICE_LED_ON,
    VOICE_LED_OFF,
    VOICE_RED,
    VOICE_GREEN,
    VOICE_BLUE,
    VOICE_RELAY_ON,
    VOICE_RELAY_OFF,
    VOICE_BEEP_ON,
    VOICE_BEEP_OFF
} VoiceCmd;

void     Voice_Init(void);
VoiceCmd Voice_Poll(void);
uint16_t Voice_GetRxCount(void);

#endif
