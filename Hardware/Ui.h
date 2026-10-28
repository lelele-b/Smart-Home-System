#ifndef __UI_H
#define __UI_H

#include "stm32f10x.h"

/*
 * 显示界面层（基于厂商 bsp_lcd.c 的 TFTLCD 驱动）
 *
 * 屏幕：240 x 320 竖屏，RGB565，STM32F103ZET6 用 FSMC 的 NOR/SRAM Bank1
 *       Sector4 驱动，命令/数据靠 A10 地址线区分（PG12）。
 *
 * 界面布局：
 *   ┌──────────────────────────────┐
 *   │  标题栏：SMART HOME SYSTEM   │  y = 0..28
 *   ├──────────────┬───────────────┤
 *   │ 温度图标 80x80│ 湿度图标 80x80│  y = 40..120
 *   │ T:25.6C      │ H:60 %        │  y = 124
 *   ├──────────────┼───────────────┤
 *   │ 光照图标 80x80│ 烟雾图标 80x80│  y = 150..230
 *   │ L:1234       │ S:0456        │  y = 234
 *   ├──────────────┴───────────────┤
 *   │ WiFi:OK  CLOUD:OK            │  y = 292
 *   └──────────────────────────────┘
 */

/* 界面初始化：清屏、画标题栏、画 4 个传感器图标和静态标签 */
void Ui_Init(void);

/*
 * 刷新传感器数值
 *   tem_x10 : 温度 x10（单位 0.1℃，例如 256 表示 25.6℃）
 *   hum     : 相对湿度 %RH（0~100）
 *   light   : 光照 ADC 原始值（0~4095，值越小光越强）
 *   smoke   : 烟雾 ADC 原始值（0~4095，值越大浓度越高）
 */
void Ui_UpdateData(int16_t tem_x10, uint8_t hum, uint16_t light, uint16_t smoke);

/* 刷新底部联网状态：wifi_ok / cloud_ok 为 1 表示正常 */
void Ui_UpdateCloud(uint8_t wifi_ok, uint8_t cloud_ok);

/* 刷新底部一行文字（给语音识别结果之类的短提示用，最长 20 个字符） */
void Ui_UpdateTip(const char *tip);

#endif
