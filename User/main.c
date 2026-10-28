#include "stm32f10x.h"
#include "stdio.h"
#include "string.h"

#include "Delay.h"
#include "Serial.h"
#include "LED.h"
#include "Beep.h"
#include "RGB.h"
#include "Relay.h"
#include "Key.h"
#include "bsp_lcd.h"
#include "Ui.h"
#include "DHT.h"
#include "ADC.h"
#include "Voice.h"
#include "WiFi.h"

/*
 * ============================================================================
 *  基于 STM32 的智能云家居系统
 *  阶段 P5：ESP8266 + 巴法云（本地上行数据、云端下行控制）
 *
 *  当前进度：
 *    P1  LED / 蜂鸣器 / RGB / 继电器 / 按键 / 调试串口        —— 已完成
 *    P2  TFT-LCD 界面（标题栏 + 4 个图标 + 数值 + 状态行）     —— 已完成
 *    P3  DHT11 温湿度 + 光照 + 烟雾采集并显示                 —— 已完成
 *    P4  SU03T 语音控制本地设备                               —— 已完成
 *    P5  巴法云上下行（本文件）                               —— 本次
 *
 *  引脚：DHT11 = PG11，光照 = PA5，烟雾 = PC1，
 *        语音 TX->PD2 / RX->PC12（UART5，9600），
 *        ESP8266 TX->PB11 / RX->PB10（USART3，115200），RST = PE6
 *  屏幕：240 x 320 竖屏，FSMC 并口驱动（厂商 bsp_lcd.c）
 *  调试：USART1 115200 8N1，printf 直接输出日志
 *
 *  注意：Hardware\WiFi.h 里的热点名称、密码、巴法云 UID 必须先改成自己的，
 *        否则程序会打印 "placeholder config" 并跳过联网（本地功能照常可用）
 * ============================================================================
 */

#define TASK_KEY_MS     20U         /* 按键扫描周期 */
#define TASK_VOICE_MS   100U        /* 语音命令轮询周期 */
#define TASK_UI_MS      500U        /* 屏幕状态行刷新周期 */
#define TASK_SENSOR_MS  2000U       /* 传感器采集周期（DHT11 不能读太快） */
#define TASK_CLOUD_MS   5000U       /* 上传巴法云的周期 */
#define TASK_CTRL_MS    50U         /* 查云端下发命令的周期 */
#define TASK_RETRY_MS   30000U      /* 断网重连间隔 */
#define BEEP_ON_MS      40U         /* 按键提示音长度 */
#define KEY4_LED_HOLD   1000U       /* KEY4 全亮保持时间 */

/* 超限报警阈值（改这里就能调灵敏度） */
#define ALARM_SMOKE_TH  120U        /* 烟雾报警阈值，ADC 采样值 0~4095 */
#define ALARM_SMOKE_HYST 20U        /* 烟雾回差：低于 阈值-20 才解除报警 */
#define ALARM_TEM_TH    350         /* 温度报警阈值，单位 0.1℃，350=35.0℃ */
#define ALARM_TEM_HYST  20          /* 温度回差：低于 350-20 才解除报警 */

/* 超限报警状态：任意一位为 1 就表示处于报警态 */
static uint8_t s_alarm_smoke = 0U;
static uint8_t s_alarm_tem   = 0U;

static uint8_t Alarm_Active(void);
static void    Alarm_Update(void);

/* 本地设备状态（同时决定上传给云端的 4 个标志位） */
static uint8_t s_led1_on   = 0U;
static uint8_t s_beep_on   = 0U;
static uint8_t s_rgb_color = RGB_OFF;
static uint8_t s_relay_on  = 0U;

/* 联网状态，屏幕底部那行显示用 */
static uint8_t s_wifi_ok  = 0U;
static uint8_t s_cloud_ok = 0U;

/* 最近一次采集到的有效数据，DHT11 读失败时屏幕上保留上一次的值 */
static DhtData  s_dht      = {60U, 256};
static uint16_t s_light    = 0U;
static uint16_t s_smoke    = 0U;
static uint8_t  s_dht_ok   = 0U;
static uint32_t s_dht_err  = 0U;

/*
 * 函数：Board_SelfTest
 * 功能：上电自检，把每个外设点亮一遍，方便肉眼确认硬件是否正常
 */
static void Board_SelfTest(void)
{
    uint8_t i;

    printf("\r\n===== Smart Home P5: CLOUD =====\r\n");

    for(i = 0U; i < 2U; i++)
    {
        LED1_ON();      Delay_ms(80);
        LED2_ON();      Delay_ms(80);
        LED3_ON();      Delay_ms(80);
        LED4_ON();      Delay_ms(80);
        LED_AllOff();   Delay_ms(120);
    }

    RGB_SetColor(RGB_RED);      Delay_ms(150);
    RGB_SetColor(RGB_GREEN);    Delay_ms(150);
    RGB_SetColor(RGB_BLUE);     Delay_ms(150);
    RGB_SetColor(RGB_OFF);

    Beep_ON();      Delay_ms(80);
    Beep_OFF();

    Relay_ON();     Delay_ms(200);
    Relay_OFF();

    printf("SelfTest Done.\r\n");
}

/*
 * 函数：Sensor_Update
 * 功能：读一次 DHT11，再取一次光照/烟雾的平均值，并同步到屏幕和串口
 */
static void Sensor_Update(void)
{
    uint8_t ret;

    ret = DHT_Read(&s_dht);
    if(ret == DHT_OK)
    {
        s_dht_ok  = 1U;
        s_dht_err = 0U;
    }
    else
    {
        s_dht_ok = 0U;
        s_dht_err++;
        /* 失败时让温度湿度保持上一次的值，避免屏幕跳成 0 */
    }

    ADC_Update();
    s_light = ADC_GetLight();
    s_smoke = ADC_GetSmoke();

    Ui_UpdateData(s_dht.tem_x10, s_dht.hum, s_light, s_smoke);

    printf("DHT %s hum=%d%% tem=%d.%dC  LIGHT=%04d SMOKE=%04d  err=%d\r\n",
           s_dht_ok ? "OK " : "ERR",
           (int)s_dht.hum,
           (int)(s_dht.tem_x10 / 10), (int)(s_dht.tem_x10 % 10),
           (int)s_light, (int)s_smoke, (int)s_dht_err);

    Alarm_Update();
}

/*
 * 函数：Device_SetAll
 * 功能：把 LED / 蜂鸣器 / RGB / 继电器 一次性设到指定状态
 * 说明：语音、按键、云端三条控制路径最后都走这个函数，
 *       保证本地状态变量和硬件实际状态永远一致
 */
static void Device_SetAll(uint8_t led, uint8_t beep, uint8_t rgb, uint8_t relay)
{
    s_led1_on   = led ? 1U : 0U;
    s_beep_on   = beep ? 1U : 0U;
    s_rgb_color = (uint8_t)(rgb % 4U);
    s_relay_on  = relay ? 1U : 0U;

    LED_Set(0, s_led1_on);

    if((s_beep_on != 0U) || (Alarm_Active() != 0U))
    {
        Beep_ON();
    }
    else
    {
        Beep_OFF();
    }

    if(Alarm_Active() != 0U)
    {
        RGB_SetColor(RGB_RED);      /* 报警中强制红灯 */
    }
    else
    {
        RGB_SetColor(s_rgb_color);
    }

    if(s_relay_on)
    {
        Relay_ON();
    }
    else
    {
        Relay_OFF();
    }
}

/*
 * 函数：Voice_Handler
 * 功能：把识别到的语音命令翻译成动作，同时在屏幕提示行显示这句话
 */
static void Voice_Handler(VoiceCmd cmd)
{
    char tip[20];

    switch(cmd)
    {
        case VOICE_TEM:
            printf("VOICE -> 问温度 : %d.%d C\r\n", (int)(s_dht.tem_x10 / 10), (int)(s_dht.tem_x10 % 10));
            sprintf(tip, "NOW TEMP %d.%dC", (int)(s_dht.tem_x10 / 10), (int)(s_dht.tem_x10 % 10));
            Ui_UpdateTip(tip);
            break;

        case VOICE_HUM:
            printf("VOICE -> 问湿度 : %d %%\r\n", (int)s_dht.hum);
            sprintf(tip, "NOW HUM  %d%%", (int)s_dht.hum);
            Ui_UpdateTip(tip);
            break;

        case VOICE_LIGHT:
            printf("VOICE -> 问光照 : %d\r\n", (int)s_light);
            sprintf(tip, "NOW LIGHT %04d", (int)s_light);
            Ui_UpdateTip(tip);
            break;

        case VOICE_SMOKE:
            printf("VOICE -> 问烟雾 : %d\r\n", (int)s_smoke);
            sprintf(tip, "NOW SMOKE %04d", (int)s_smoke);
            Ui_UpdateTip(tip);
            break;

        case VOICE_LED_ON:
            Device_SetAll(1U, s_beep_on, s_rgb_color, s_relay_on);
            printf("VOICE -> 开灯\r\n");
            Ui_UpdateTip("VOICE LED ON");
            break;

        case VOICE_LED_OFF:
            Device_SetAll(0U, s_beep_on, s_rgb_color, s_relay_on);
            printf("VOICE -> 关灯\r\n");
            Ui_UpdateTip("VOICE LED OFF");
            break;

        case VOICE_RED:
            Device_SetAll(s_led1_on, s_beep_on, RGB_RED, s_relay_on);
            printf("VOICE -> 红灯\r\n");
            Ui_UpdateTip("VOICE RGB RED");
            break;

        case VOICE_GREEN:
            Device_SetAll(s_led1_on, s_beep_on, RGB_GREEN, s_relay_on);
            printf("VOICE -> 绿灯\r\n");
            Ui_UpdateTip("VOICE RGB GREEN");
            break;

        case VOICE_BLUE:
            Device_SetAll(s_led1_on, s_beep_on, RGB_BLUE, s_relay_on);
            printf("VOICE -> 蓝灯\r\n");
            Ui_UpdateTip("VOICE RGB BLUE");
            break;

        case VOICE_RELAY_ON:
            Device_SetAll(s_led1_on, s_beep_on, s_rgb_color, 1U);
            printf("VOICE -> 开继电器\r\n");
            Ui_UpdateTip("VOICE RELAY ON");
            break;

        case VOICE_RELAY_OFF:
            Device_SetAll(s_led1_on, s_beep_on, s_rgb_color, 0U);
            printf("VOICE -> 关继电器\r\n");
            Ui_UpdateTip("VOICE RELAY OFF");
            break;

        case VOICE_BEEP_ON:
            Device_SetAll(s_led1_on, 1U, s_rgb_color, s_relay_on);
            printf("VOICE -> 开蜂鸣器\r\n");
            Ui_UpdateTip("VOICE BEEP ON");
            break;

        case VOICE_BEEP_OFF:
            Device_SetAll(s_led1_on, 0U, s_rgb_color, s_relay_on);
            printf("VOICE -> 关蜂鸣器\r\n");
            Ui_UpdateTip("VOICE BEEP OFF");
            break;

        default:
            break;
    }
}

/*
 * 函数：Cloud_Handle
 * 功能：解析云端下发的控制命令并执行，执行完立即回传一次状态
 */
static void Cloud_Handle(void)
{
    uint8_t led   = s_led1_on;
    uint8_t beep  = s_beep_on;
    uint8_t rgb   = s_rgb_color;
    uint8_t relay = s_relay_on;
    uint8_t mask;

    mask = WiFi_GetControl(&led, &beep, &rgb, &relay);
    if(mask == 0U)
    {
        return;
    }

    Device_SetAll(led, beep, rgb, relay);

    printf("CLOUD -> led=%d beep=%d rgb=%d relay=%d (mask=%02X)\r\n",
           (int)s_led1_on, (int)s_beep_on, (int)s_rgb_color, (int)s_relay_on, (int)mask);

    Ui_UpdateTip("CLOUD CTRL OK");

    /* 马上回传一次，小程序上看到的状态就同步了 */
    WiFi_PublishData(s_dht.tem_x10, s_dht.hum, s_light, s_smoke,
                     s_led1_on, (uint8_t)(s_beep_on || (Alarm_Active() != 0U)), s_rgb_color, s_relay_on);
}

int main(void)
{
    uint32_t now;
    uint32_t t_key       = 0U;
    uint32_t t_voice     = 0U;
    uint32_t t_ui        = 0U;
    uint32_t t_sensor    = 0U;
    uint32_t t_cloud     = 0U;
    uint32_t t_ctrl      = 0U;
    uint32_t t_retry     = 0U;
    uint32_t beep_off_at = 0U;
    uint32_t led_off_at  = 0U;
    uint8_t  ret;
    KeyId    key;
    VoiceCmd cmd;

    Delay_Init();               /* SysTick 1ms 节拍 */
    Serial_Init();              /* USART1 115200，printf 可用 */
    LED_Init();                 /* PE2~PE5 低电平点亮 */
    Beep_Init();                /* PC0 高电平鸣叫 */
    RGB_Init();                 /* PA8 红 / PA7 绿 / PA6 蓝，低电平点亮 */
    Relay_Init();               /* PA11 高电平吸合 */
    Key_Init();                 /* PA0 高有效，PC4/PC5/PC6 低有效 */
    DHT_Init();                 /* PG11 单总线 */
    ADC_Init_Multi();           /* PA5 光照 / PC1 烟雾，ADC1+DMA */
    Voice_Init();               /* UART5 9600，中断接收 */
    WiFi_Init();                /* USART3 115200 + PE6 复位脚 */

    printf("LCD init start...\r\n");
    TFTLCD_Init();              /* FSMC 并口 + 背光 PB0 */
    Ui_Init();                  /* 标题栏、4 个图标、初始数值 */
    printf("LCD init done.\r\n");

    Board_SelfTest();

    Sensor_Update();            /* 先采一次，屏幕上马上有数据 */

    /* 配置没填之前不联网，避免每次重连都卡十几秒 */
    if(WiFi_ConfigReady() == 0U)
    {
        printf("WIFI: config not ready, skip connect (edit Hardware/WiFi.h)\r\n");
        Ui_UpdateTip("SET WIFI CONFIG");
    }
    else
    {
        printf("WIFI connecting...\r\n");
        ret = WiFi_Connect();
        s_wifi_ok  = (ret == WIFI_OK) ? 1U : 0U;
        s_cloud_ok = s_wifi_ok;
        printf("WIFI connect ret=%d\r\n", (int)ret);
        Ui_UpdateTip(s_wifi_ok ? "CLOUD READY" : "CLOUD FAIL");
    }

    Ui_UpdateCloud(s_wifi_ok, s_cloud_ok);

    while(1)
    {
        now = Delay_GetTick();

        /* ---------------- 任务 1：按键扫描 20ms ---------------- */
        if((now - t_key) >= TASK_KEY_MS)
        {
            t_key = now;

            key = Key_Scan();

            if(key != KEY_NONE)
            {
                Beep_ON();
                beep_off_at = now + BEEP_ON_MS;

                switch(key)
                {
                    case KEY1:
                        s_led1_on = (uint8_t)(s_led1_on ? 0U : 1U);
                        LED_Set(0, s_led1_on);
                        printf("KEY1 -> LED1 %s\r\n", s_led1_on ? "ON" : "OFF");
                        Ui_UpdateTip(s_led1_on ? "KEY1 LED1 ON " : "KEY1 LED1 OFF");
                        break;

                    case KEY2:
                        Device_SetAll(s_led1_on, s_beep_on, s_rgb_color,
                                      (uint8_t)(s_relay_on ? 0U : 1U));
                        printf("KEY2 -> RELAY %s\r\n", s_relay_on ? "ON" : "OFF");
                        Ui_UpdateTip(s_relay_on ? "KEY2 RELAY ON " : "KEY2 RELAY OFF");
                        break;

                    case KEY3:
                        Device_SetAll(s_led1_on, s_beep_on,
                                      (uint8_t)((s_rgb_color + 1U) % 4U), s_relay_on);
                        printf("KEY3 -> RGB %d\r\n", (int)s_rgb_color);
                        Ui_UpdateTip("KEY3 RGB CHANGE");
                        break;

                    case KEY4:
                        LED1_ON(); LED2_ON(); LED3_ON(); LED4_ON();
                        led_off_at = now + KEY4_LED_HOLD;
                        printf("KEY4 -> ALL LED ON\r\n");
                        Ui_UpdateTip("KEY4 ALL LED ON");
                        break;

                    default:
                        break;
                }
            }
        }

        /* ---------------- 任务 2：语音命令 100ms ---------------- */
        if((now - t_voice) >= TASK_VOICE_MS)
        {
            t_voice = now;

            cmd = Voice_Poll();
            if(cmd != VOICE_NONE)
            {
                Voice_Handler(cmd);
            }
        }

        /* ---------------- 任务 3：云端下发命令 50ms ---------------- */
        if((now - t_ctrl) >= TASK_CTRL_MS)
        {
            t_ctrl = now;
            Cloud_Handle();
        }

        /* ---------------- 任务 4：采集传感器 2000ms ---------------- */
        if((now - t_sensor) >= TASK_SENSOR_MS)
        {
            t_sensor = now;
            Sensor_Update();
        }

        /* ---------------- 任务 5：上传云端 5000ms ---------------- */
        if((now - t_cloud) >= TASK_CLOUD_MS)
        {
            t_cloud = now;

            if(s_cloud_ok)
            {
                WiFi_PublishData(s_dht.tem_x10, s_dht.hum, s_light, s_smoke,
                                 s_led1_on, (uint8_t)(s_beep_on || (Alarm_Active() != 0U)), s_rgb_color, s_relay_on);
            }
        }

        /* ---------------- 任务 6：刷新屏幕状态行 500ms ---------------- */
        if((now - t_ui) >= TASK_UI_MS)
        {
            t_ui = now;
            Ui_UpdateCloud(s_wifi_ok, s_cloud_ok);

            /* 报警期间提示行常驻报警信息，盖掉语音/云端的临时提示 */
            if(s_alarm_smoke != 0U)
            {
                Ui_UpdateTip("!! SMOKE ALARM");
            }
            else if(s_alarm_tem != 0U)
            {
                Ui_UpdateTip("!! TEMP ALARM");
            }
        }

        /* ---------------- 任务 7：断网重连 30s ---------------- */
        if((s_cloud_ok == 0U) &&
           (WiFi_ConfigReady() != 0U) &&
           ((now - t_retry) >= TASK_RETRY_MS))
        {
            t_retry = now;

            printf("WIFI reconnecting...\r\n");
            ret = WiFi_Connect();
            s_wifi_ok  = (ret == WIFI_OK) ? 1U : 0U;
            s_cloud_ok = s_wifi_ok;
            Ui_UpdateTip(s_wifi_ok ? "CLOUD READY" : "CLOUD FAIL");
        }

        /* ---------------- 任务 8：到点关蜂鸣器 ---------------- */
        if((beep_off_at != 0U) && ((int32_t)(now - beep_off_at) >= 0))
        {
            beep_off_at = 0U;
            if((s_beep_on == 0U) && (Alarm_Active() == 0U))
            {
                Beep_OFF();
            }
        }

        /* ---------------- 任务 9：到点关 KEY4 的全亮 ---------------- */
        if((led_off_at != 0U) && ((int32_t)(now - led_off_at) >= 0))
        {
            led_off_at = 0U;
            LED_AllOff();
            LED_Set(0, s_led1_on);
        }
    }
}

/*
 * 函数：Alarm_Active
 * 功能：返回当前是否处于超限报警态，1=报警中，0=正常
 */
static uint8_t Alarm_Active(void)
{
    return (uint8_t)(s_alarm_smoke || s_alarm_tem);
}

/*
 * 函数：Alarm_Update
 * 功能：判断烟雾 / 温度有没有超阈值，只在状态跳变的那一刻动作
 * 说明：带迟滞（回差），数值在阈值附近来回抖时不会一直响
 */
static void Alarm_Update(void)
{
    uint8_t old_smoke = s_alarm_smoke;
    uint8_t old_tem   = s_alarm_tem;

    /* 烟雾：到了阈值就报警，掉到 阈值-回差 以下才解除 */
    if(s_smoke >= ALARM_SMOKE_TH)
    {
        s_alarm_smoke = 1U;
    }
    else if(((uint32_t)s_smoke + ALARM_SMOKE_HYST) < ALARM_SMOKE_TH)
    {
        s_alarm_smoke = 0U;
    }
    else
    {
        /* 落在回差区间里，保持原状态 */
    }

    /* 温度：单位 0.1℃，判断方式同上 */
    if(s_dht.tem_x10 >= ALARM_TEM_TH)
    {
        s_alarm_tem = 1U;
    }
    else if(((int32_t)s_dht.tem_x10 + ALARM_TEM_HYST) < ALARM_TEM_TH)
    {
        s_alarm_tem = 0U;
    }
    else
    {
        /* 落在回差区间里，保持原状态 */
    }

    /* 状态没变就什么都不做，避免每 2 秒重复鸣叫 */
    if((s_alarm_smoke == old_smoke) && (s_alarm_tem == old_tem))
    {
        return;
    }

    if(s_alarm_smoke != 0U)
    {
        printf("ALARM -> SMOKE OVER LIMIT  smoke=%d  th=%d\r\n", (int)s_smoke, (int)ALARM_SMOKE_TH);
    }
    if(s_alarm_tem != 0U)
    {
        printf("ALARM -> TEMP OVER LIMIT  tem=%d.%dC  th=%d.%dC\r\n",
               (int)(s_dht.tem_x10 / 10), (int)(s_dht.tem_x10 % 10),
               (int)(ALARM_TEM_TH / 10), (int)(ALARM_TEM_TH % 10));
    }

    if(Alarm_Active() != 0U)
    {
        /* 报警中：蜂鸣器长鸣 + RGB 红灯，安全优先，手动关也不压掉 */
        Beep_ON();
        RGB_SetColor(RGB_RED);
    }
    else
    {
        printf("ALARM -> CLEAR\r\n");
        if(s_beep_on == 0U)
        {
            Beep_OFF();
        }
        RGB_SetColor(s_rgb_color);
    }
}
