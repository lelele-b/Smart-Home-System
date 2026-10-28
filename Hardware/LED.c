#include "LED.h"

/* PE2~PE5 对应 LED1~LED4，硬件为低电平点亮 */
#define LED_PORT      GPIOE
#define LED_ALL_PIN   (GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5)

/* 四路 LED 初始化：推挽输出 50MHz，上电默认全灭 */
void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = LED_ALL_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &GPIO_InitStructure);

    LED_AllOff();       /* 上电先熄灭，避免复位瞬间乱闪 */
}

/* 第 idx 路（0~3）开关：低电平点亮，高电平熄灭 */
void LED_Set(uint8_t idx, uint8_t on)
{
    uint16_t pin;

    if(idx >= LED_NUM)
    {
        return;
    }

    pin = (uint16_t)(GPIO_Pin_2 << idx);    /* idx=0 -> PE2，idx=3 -> PE5 */

    if(on)
    {
        GPIO_ResetBits(LED_PORT, pin);
    }
    else
    {
        GPIO_SetBits(LED_PORT, pin);
    }
}

void LED_AllOff(void)   { GPIO_SetBits(LED_PORT, LED_ALL_PIN); }

void LED1_ON(void)      { GPIO_ResetBits(LED_PORT, GPIO_Pin_2); }
void LED1_OFF(void)     { GPIO_SetBits  (LED_PORT, GPIO_Pin_2); }

void LED2_ON(void)      { GPIO_ResetBits(LED_PORT, GPIO_Pin_3); }
void LED2_OFF(void)     { GPIO_SetBits  (LED_PORT, GPIO_Pin_3); }

void LED3_ON(void)      { GPIO_ResetBits(LED_PORT, GPIO_Pin_4); }
void LED3_OFF(void)     { GPIO_SetBits  (LED_PORT, GPIO_Pin_4); }

void LED4_ON(void)      { GPIO_ResetBits(LED_PORT, GPIO_Pin_5); }
void LED4_OFF(void)     { GPIO_SetBits  (LED_PORT, GPIO_Pin_5); }
