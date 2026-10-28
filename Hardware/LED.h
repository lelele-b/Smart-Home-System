#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

/* 四路 LED 接 PE2~PE5，低电平点亮 */
#define LED_NUM     4U

/* 初始化 PE2~PE5 为推挽输出，上电全部熄灭 */
void LED_Init(void);

/* 第 idx 路（0~3）单独开关，on 非 0 点亮 */
void LED_Set(uint8_t idx, uint8_t on);

/* 四路全部熄灭 */
void LED_AllOff(void);

/* 逐路控制，方便阅读 */
void LED1_ON(void);
void LED1_OFF(void);
void LED2_ON(void);
void LED2_OFF(void);
void LED3_ON(void);
void LED3_OFF(void);
void LED4_ON(void);
void LED4_OFF(void);

#endif
