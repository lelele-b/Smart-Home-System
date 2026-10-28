#ifndef __SERIAL_H
#define __SERIAL_H

#include "stm32f10x.h"

/*
 * 调试串口 USART1：PA9(TX) / PA10(RX)，115200 8N1
 * 只做发送，配合 printf 打印运行日志，
 * 方便观察 WiFi 连接、云端收发、传感器采集是否正常。
 */

void Serial_Init(void);

void Serial_SendByte(uint8_t byte);

void Serial_SendString(const char *str);

#endif
