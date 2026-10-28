#ifndef __DHT_H
#define __DHT_H

#include "stm32f10x.h"

/*
 * DHT11 温湿度传感器（单总线）
 * 接线：DATA -> PG11，模块自带 4.7K~10K 上拉电阻
 *
 * 通信过程：
 *   1. 主机拉低至少 18ms 作为起始信号，然后释放总线
 *   2. DHT11 响应：拉低约 80us，再拉高约 80us
 *   3. 随后连续发送 40 位数据：
 *        湿度整数(8) 湿度小数(8) 温度整数(8) 温度小数(8) 校验和(8)
 *      每一位都是先拉低约 50us，再拉高表示数据：
 *        高电平持续约 26~28us 表示 0，约 70us 表示 1
 *
 * 注意：时序要求严格，读取期间必须关掉 SysTick 中断，
 *       否则 1ms 一次的中断会把位宽拉长导致读出乱码
 */

typedef struct
{
    uint8_t hum;            /* 湿度，整数 %RH */
    int16_t tem_x10;        /* 温度 x10，例如 256 表示 25.6℃ */
} DhtData;

#define DHT_OK          0U      /* 读取成功 */
#define DHT_ERR_TIMEOUT 1U      /* 传感器没有响应 */
#define DHT_ERR_CHECK   2U      /* 校验和不正确 */

void    DHT_Init(void);
uint8_t DHT_Read(DhtData *out);

#endif
