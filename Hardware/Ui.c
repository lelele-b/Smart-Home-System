#include "Ui.h"
#include "bsp_lcd.h"
#include "stdio.h"

/*
 * 图标资源：由 Image2Lcd 取模得到的 80x80 16 位真彩色数组，
 * 每个数组 80*80*2 = 12800 字节，存放在同目录的 tem_1.h 等头文件里。
 * 只在本文件里包含，避免重复定义。
 */
#include "tem_1.h"
#include "hum_1.h"
#include "illu_1.h"
#include "mq2_1.h"

/* ---------------- 界面坐标 ---------------- */
#define UI_ICON_W        80U
#define UI_ICON_H        80U

#define UI_ICON_TEM_X    20U
#define UI_ICON_TEM_Y    40U
#define UI_ICON_HUM_X    130U
#define UI_ICON_HUM_Y    40U
#define UI_ICON_ILL_X    20U
#define UI_ICON_ILL_Y    150U
#define UI_ICON_MQ2_X    130U
#define UI_ICON_MQ2_Y    150U

#define UI_TEXT_TEM_X    20U
#define UI_TEXT_TEM_Y    124U
#define UI_TEXT_HUM_X    130U
#define UI_TEXT_HUM_Y    124U
#define UI_TEXT_ILL_X    20U
#define UI_TEXT_ILL_Y    234U
#define UI_TEXT_MQ2_X    130U
#define UI_TEXT_MQ2_Y    234U

#define UI_TIP_Y         272U
#define UI_STATUS_Y      296U

/* 标题栏颜色、数值文字用的黑底白字 */
#define UI_TITLE_COLOR   0x02B0U
#define UI_TEXT_COLOR    BLACK
#define UI_BG_COLOR      WHITE

/* 上一次显示的字符串，用来判断要不要重画，减少闪烁 */
static char s_tem[20] = "T:--.-C";
static char s_hum[20] = "H:--%";
static char s_ill[20] = "L:----";
static char s_mq2[20] = "S:----";
static char s_sta[32] = "WiFi:--  CLOUD:--";
static char s_tip[24] = "                ";

/*
 * 函数：Ui_DrawText
 * 功能：在固定位置写一串固定宽度的文字，先擦掉旧内容
 * 说明：LCD_ShowString 不会自动清掉长字符串留下的残影，
 *       所以这里先用背景色把该行填掉，再写新内容
 */
static void Ui_DrawText(uint16_t x, uint16_t y, uint16_t w, char *str)
{
    LCD_Fill(x, y, (uint16_t)(x + w), (uint16_t)(y + 16U), UI_BG_COLOR);
    LCD_ShowString(x, y, w, 16U, 16U, (uint8_t *)str);
}

/*
 * 函数：Ui_Init
 * 功能：清屏、画标题栏、把 4 个图标和静态文字画上去
 * 说明：只在开机时调用一次，图标是常量数组，画 4 张约需几百毫秒
 */
void Ui_Init(void)
{
    POINT_COLOR = UI_TEXT_COLOR;
    BACK_COLOR  = UI_BG_COLOR;

    LCD_Clear(UI_BG_COLOR);

    /* 标题栏 */
    LCD_Fill(0U, 0U, 239U, 28U, UI_TITLE_COLOR);
    POINT_COLOR = WHITE;
    LCD_ShowString(30U, 6U, 190U, 16U, 16U, (uint8_t *)"SMART HOME SYSTEM");
    POINT_COLOR = UI_TEXT_COLOR;

    /* 4 个传感器的图标 */
    LCD_ShowPicture(UI_ICON_TEM_X, UI_ICON_TEM_Y, UI_ICON_W, UI_ICON_H, (uint8_t *)gImage_tem_1);
    LCD_ShowPicture(UI_ICON_HUM_X, UI_ICON_HUM_Y, UI_ICON_W, UI_ICON_H, (uint8_t *)gImage_hum_1);
    LCD_ShowPicture(UI_ICON_ILL_X, UI_ICON_ILL_Y, UI_ICON_W, UI_ICON_H, (uint8_t *)gImage_illu_1);
    LCD_ShowPicture(UI_ICON_MQ2_X, UI_ICON_MQ2_Y, UI_ICON_W, UI_ICON_H, (uint8_t *)gImage_mq2_1);

    /* 初始占位数值 */
    Ui_DrawText(UI_TEXT_TEM_X, UI_TEXT_TEM_Y, 90U, s_tem);
    Ui_DrawText(UI_TEXT_HUM_X, UI_TEXT_HUM_Y, 90U, s_hum);
    Ui_DrawText(UI_TEXT_ILL_X, UI_TEXT_ILL_Y, 90U, s_ill);
    Ui_DrawText(UI_TEXT_MQ2_X, UI_TEXT_MQ2_Y, 90U, s_mq2);
    Ui_DrawText(0U, UI_STATUS_Y, 200U, s_sta);

}

/*
 * 函数：Ui_UpdateData
 * 参数：tem_x10 温度x10，hum 湿度，light 光照 ADC，smoke 烟雾 ADC
 * 功能：刷新 4 个传感器数值，数值没变就不重画，避免屏幕闪烁
 */
void Ui_UpdateData(int16_t tem_x10, uint8_t hum, uint16_t light, uint16_t smoke)
{
    char buf[20];
    int16_t t = tem_x10;

    /* DHT11 偶尔会读失败返回 0，这里不显示 0.0 这种明显异常的负温度 */
    if(t < 0)
    {
        t = (int16_t)(-t);
        sprintf(buf, "T:-%d.%dC", t / 10, t % 10);
    }
    else
    {
        sprintf(buf, "T:%d.%dC", t / 10, t % 10);
    }
    if((buf[0] != s_tem[0]) || (buf[2] != s_tem[2]) || (buf[4] != s_tem[4]) || (buf[6] != s_tem[6]))
    {
        Ui_DrawText(UI_TEXT_TEM_X, UI_TEXT_TEM_Y, 90U, buf);
    }
    sprintf(s_tem, "%s", buf);

    sprintf(buf, "H:%d%%", (int)hum);
    if((buf[2] != s_hum[2]) || (buf[3] != s_hum[3]) || (buf[4] != s_hum[4]))
    {
        Ui_DrawText(UI_TEXT_HUM_X, UI_TEXT_HUM_Y, 90U, buf);
    }
    sprintf(s_hum, "%s", buf);

    sprintf(buf, "L:%04d", (int)light);
    if((buf[2] != s_ill[2]) || (buf[3] != s_ill[3]) || (buf[4] != s_ill[4]) || (buf[5] != s_ill[5]))
    {
        Ui_DrawText(UI_TEXT_ILL_X, UI_TEXT_ILL_Y, 90U, buf);
    }
    sprintf(s_ill, "%s", buf);

    sprintf(buf, "S:%04d", (int)smoke);
    if((buf[2] != s_mq2[2]) || (buf[3] != s_mq2[3]) || (buf[4] != s_mq2[4]) || (buf[5] != s_mq2[5]))
    {
        Ui_DrawText(UI_TEXT_MQ2_X, UI_TEXT_MQ2_Y, 90U, buf);
    }
    sprintf(s_mq2, "%s", buf);
}

/*
 * 函数：Ui_UpdateCloud
 * 参数：wifi_ok / cloud_ok，1 表示已连接
 * 功能：刷新底部一行联网状态
 */
void Ui_UpdateCloud(uint8_t wifi_ok, uint8_t cloud_ok)
{
    char buf[32];

    sprintf(buf, "WiFi:%-2s CLOUD:%-2s", wifi_ok ? "OK" : "--", cloud_ok ? "OK" : "--");
    if((buf[5] != s_sta[5]) || (buf[6] != s_sta[6]) || (buf[16] != s_sta[16]) || (buf[17] != s_sta[17]))
    {
        Ui_DrawText(0U, UI_STATUS_Y, 220U, buf);
    }
    sprintf(s_sta, "%s", buf);
}

/*
 * 函数：Ui_UpdateTip
 * 参数：tip —— 一行最多 16 个字符的提示
 * 功能：在状态行上面一行显示简短提示（语音识别结果、告警等）
 */
void Ui_UpdateTip(const char *tip)
{
    char buf[24];
    uint8_t i;

    for(i = 0U; (i < 16U) && (tip[i] != '\0'); i++)
    {
        buf[i] = tip[i];
    }
    for(; i < 16U; i++)
    {
        buf[i] = ' ';
    }
    buf[16] = '\0';

    Ui_DrawText(0U, UI_TIP_Y, 200U, buf);
    sprintf(s_tip, "%s", buf);
}
