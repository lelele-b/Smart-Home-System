#ifndef __BEEP_H
#define __BEEP_H

#include "stm32f10x.h"

/*
 * 有源蜂鸣器接 PC0，经 S8050 三极管驱动：
 * PC0 高电平 -> 三极管饱和导通 -> 蜂鸣器响
 * PC0 低电平 -> 三极管截止     -> 蜂鸣器停
 */

/* 初始化 PC0 为推挽输出，上电默认不响 */
void Beep_Init(void);

void Beep_ON(void);
void Beep_OFF(void);

#endif
