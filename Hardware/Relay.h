#ifndef __RELAY_H
#define __RELAY_H

#include "stm32f10x.h"

/*
 * 继电器接 PA11，高电平吸合（启动），低电平断开
 * 注意：PA11/PA12 同时也是 USB 的 DM/DP，继电器占用后 USB 虚拟串口不可用，
 *       下载只能用 ST-Link(SWD) 或串口 ISP。
 */

/* 初始化 PA11 为推挽输出，上电默认断开 */
void Relay_Init(void);

void Relay_ON(void);
void Relay_OFF(void);

/* 翻转一次，供按键做“开关”效果 */
void Relay_Toggle(void);

/* 返回当前状态：1=吸合，0=断开 */
uint8_t Relay_GetState(void);

#endif
