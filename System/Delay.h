#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"

/*
 * 延时 / 计时（STM32F103ZET6，72MHz）
 *
 * 说明：厂商 bsp_lcd.c 只 include "delay.h"，并且内部有几十处
 *       Delay_ms()/Delay_us() 调用，所以这两个函数名必须保持一致。
 *
 *   Delay_Init()   : 配置 SysTick 为 1ms 节拍（中断累加）
 *   Delay_ms(ms)   : 毫秒级阻塞延时（基于 SysTick 毫秒计数）
 *   Delay_us(us)   : 微秒级阻塞延时（基于 SysTick->VAL 周期计数）
 *   Delay_GetTick(): 取上电以来的毫秒数，主循环用它做非阻塞时间片
 */

void     Delay_Init(void);
void     Delay_ms(uint32_t ms);
void     Delay_us(uint32_t us);
uint32_t Delay_GetTick(void);

#endif
