#include "Beep.h"

/* 蜂鸣器 PC0，高电平鸣叫 */
#define BEEP_PORT   GPIOC
#define BEEP_PIN    GPIO_Pin_0

void Beep_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = BEEP_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BEEP_PORT, &GPIO_InitStructure);

    Beep_OFF();         /* 上电默认不响 */
}

void Beep_ON(void)      { GPIO_SetBits  (BEEP_PORT, BEEP_PIN); }
void Beep_OFF(void)     { GPIO_ResetBits(BEEP_PORT, BEEP_PIN); }
