#ifndef __RGB_H
#define __RGB_H

#include "stm32f10x.h"

/*
 * 共阳极 RGB 全彩灯：
 *   PA8 -> 红(R)，PA7 -> 绿(G)，PA6 -> 蓝(B)
 *   公共端接 VCC3.3，所以引脚输出低电平才点亮
 *
 * 颜色编号与云端 rgb 字段、语音模块 0x03/0x04/0x05 保持一致：
 *   0=全灭  1=红  2=绿  3=蓝
 */

#define RGB_OFF     0U
#define RGB_RED     1U
#define RGB_GREEN   2U
#define RGB_BLUE    3U

/* 初始化 PA6/PA7/PA8 为推挽输出，上电全灭 */
void RGB_Init(void);

/* 按颜色编号设置，超出范围按全灭处理 */
void RGB_SetColor(uint8_t color);

#endif
