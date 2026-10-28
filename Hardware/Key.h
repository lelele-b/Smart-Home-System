#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

/*
 * 四个独立按键（信盈达 ZET6 开发板）：
 *   KEY1 -> PA0  按下为高电平（内部下拉，按下接 VCC3.3）
 *   KEY2 -> PC4  按下为低电平（内部上拉，按下接 GND）
 *   KEY3 -> PC5  按下为低电平
 *   KEY4 -> PC6  按下为低电平
 *
 * 采用“非阻塞扫描”：由主循环每 20ms 调一次 Key_Scan()，
 * 内部连续 2 次采样一致才认账（约 40ms 消抖），
 * 不再用 while 等待松手，主循环不会被卡住。
 */

typedef enum
{
    KEY_NONE = 0,       /* 本次没有新的按下事件 */
    KEY1     = 1,
    KEY2     = 2,
    KEY3     = 3,
    KEY4     = 4
} KeyId;

/* 配置四个引脚为输入（PA0 下拉，PC4/PC5/PC6 上拉） */
void Key_Init(void);

/* 每 20ms 调用一次；返回本次“新按下”的键，没有则返回 KEY_NONE */
KeyId Key_Scan(void);

/* 查询某个键当前是否处于按下状态（1=按下） */
uint8_t Key_IsPressed(KeyId key);

#endif
