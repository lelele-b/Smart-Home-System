#ifndef __WIFI_H
#define __WIFI_H

#include "stm32f10x.h"

/*
 * ESP8266 WiFi 模块 + 巴法云（bemfa.com）接入
 * 接线：模块 TX -> PB11 (USART3_RX)，模块 RX -> PB10 (USART3_TX)
 *       PE6 -> 模块 RST（低电平复位）
 *
 * 巴法云 TCP 服务：bemfa.com:8344，透传模式（AT+CIPMODE=1）
 * 两个主题必须先在巴法云控制台建好，名字不能带中文，
 * 而且结尾三位必须是设备类型码（006=开关，004=传感器）：
 *   devicectrl006  —— 小程序下发控制命令
 *   sensordata004  —— 单片机上传传感器数据
 * 不带类型码的主题会被服务器静默忽略（订阅/发布都回 res=0）。
 *
 * 数据格式（顺序固定，两端必须一致）：
 *   上传：#温度#湿度#光照#烟雾#led#beep#rgb#relay#
 *   下发：led:1 / beep:0 / rgb:2 / relay:1
 *
 * ！！！注意！！！
 * 下面三项换成自己的：WiFi 热点名称/密码（必须是 2.4G 热点）、
 * 巴法云私钥 UID（控制台左侧"私钥"那一串 32 位十六进制）。
 * 换好以后重新编译烧录，小程序里也要填同一个 UID。
 */

#define WIFI_SSID           "YOUR_WIFI_NAME"         /* 手机热点名（必须开 2.4G 频段） */
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"     /* 手机热点密码 */
#define WIFI_BEMFA_UID      "YOUR_BEMFA_UID"          /* 巴法云私钥 UID */

#define WIFI_SERVER         "bemfa.com"
#define WIFI_PORT           8344U
#define WIFI_TOPIC_SUB      "devicectrl006"
#define WIFI_TOPIC_PUB      "sensordata004"

#define WIFI_RX_SIZE        600U                    /* 接收缓冲大小 */

/* WiFi_Connect 的返回值 */
#define WIFI_OK             0U                      /* 全部成功，已进入透传 */
#define WIFI_ERR_AT         1U                      /* AT 不通，模块没接好 */
#define WIFI_ERR_JOIN       2U                      /* 连不上热点 */
#define WIFI_ERR_TCP        3U                      /* 连不上巴法云服务器 */
#define WIFI_ERR_SEND       4U                      /* 进不了透传模式 */

/* 云端下发命令里解析出的字段（按位或起来告诉主循环哪几项变了） */
#define WIFI_CTRL_LED       0x01U
#define WIFI_CTRL_BEEP      0x02U
#define WIFI_CTRL_RGB       0x04U
#define WIFI_CTRL_RELAY     0x08U

uint8_t WiFi_ConfigReady(void);          /* 1 = 热点和 UID 都填好了 */
void    WiFi_Init(void);
uint8_t WiFi_Connect(void);
uint8_t WiFi_IsConnected(void);
void    WiFi_PublishData(int16_t tem_x10, uint8_t hum, uint16_t light, uint16_t smoke,
                         uint8_t led, uint8_t beep, uint8_t rgb, uint8_t relay);
uint8_t WiFi_GetControl(uint8_t *led, uint8_t *beep, uint8_t *rgb, uint8_t *relay);

#endif

