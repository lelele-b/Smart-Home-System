#ifndef __ADC_H
#define __ADC_H

#include "stm32f10x.h"

/*
 * 两路模拟量采集（ADC1 + DMA1 通道 1，循环搬运）
 *   光照传感器 LM393 -> PA5  -> ADC1_IN5  （通道 5，规则组序号 1）
 *   MQ2 烟雾传感器   -> PC1  -> ADC1_IN11 （通道 11，规则组序号 2）
 *
 * DMA 把 2 个通道的结果交替搬进 20 个半字的缓冲区：
 *   buf[0]=光照 buf[1]=烟雾 buf[2]=光照 buf[3]=烟雾 ...
 * 取值时把 10 组累加求平均，滤掉电源纹波带来的跳动。
 *
 * 注意：LM393 模块输出的是比较器结果，光照越强采集值越小；
 *       MQ2 是气敏电阻，烟雾浓度越大采集值越大
 */

#define ADC_LIGHT_CH    ADC_Channel_5
#define ADC_SMOKE_CH    ADC_Channel_11

void     ADC_Init_Multi(void);
void     ADC_Update(void);
uint16_t ADC_GetLight(void);
uint16_t ADC_GetSmoke(void);

#endif
