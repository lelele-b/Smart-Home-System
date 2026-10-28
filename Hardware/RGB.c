#include "RGB.h"

/* 引脚分配：红=PA8，绿=PA7，蓝=PA6 */
#define RGB_PORT    GPIOA
#define RGB_R_PIN   GPIO_Pin_8
#define RGB_G_PIN   GPIO_Pin_7
#define RGB_B_PIN   GPIO_Pin_6
#define RGB_ALL_PIN (RGB_R_PIN | RGB_G_PIN | RGB_B_PIN)

/* 三路推挽输出，上电全灭 */
void RGB_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = RGB_ALL_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(RGB_PORT, &GPIO_InitStructure);

    RGB_SetColor(RGB_OFF);
}

/* 0=全灭，1=红，2=绿，3=蓝；共阳接法，低电平点亮 */
void RGB_SetColor(uint8_t color)
{
    /* 先全部熄灭（三路都输出高电平） */
    GPIO_SetBits(RGB_PORT, RGB_ALL_PIN);

    switch(color)
    {
        case RGB_RED:   GPIO_ResetBits(RGB_PORT, RGB_R_PIN); break;
        case RGB_GREEN: GPIO_ResetBits(RGB_PORT, RGB_G_PIN); break;
        case RGB_BLUE:  GPIO_ResetBits(RGB_PORT, RGB_B_PIN); break;
        default:        break;                               /* 全灭 */
    }
}
