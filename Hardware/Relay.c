#include "Relay.h"

/* 继电器 PA11，高电平吸合 */
#define RELAY_PORT  GPIOA
#define RELAY_PIN   GPIO_Pin_11

static uint8_t s_relay_state = 0U;      /* 0=断开，1=吸合 */

void Relay_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = RELAY_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(RELAY_PORT, &GPIO_InitStructure);

    Relay_OFF();        /* 上电默认断开，避免误吸合 */
}

void Relay_ON(void)
{
    GPIO_SetBits(RELAY_PORT, RELAY_PIN);
    s_relay_state = 1U;
}

void Relay_OFF(void)
{
    GPIO_ResetBits(RELAY_PORT, RELAY_PIN);
    s_relay_state = 0U;
}

/* 翻转：吸合 -> 断开，断开 -> 吸合 */
void Relay_Toggle(void)
{
    if(s_relay_state)
    {
        Relay_OFF();
    }
    else
    {
        Relay_ON();
    }
}

uint8_t Relay_GetState(void)
{
    return s_relay_state;
}
