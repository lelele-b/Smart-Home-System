#include "Key.h"

/* 读到的是“按下”的有效电平：KEY1 高有效，KEY2~KEY4 低有效 */
#define KEY1_PORT   GPIOA
#define KEY1_PIN    GPIO_Pin_0

#define KEY234_PORT GPIOC
#define KEY2_PIN    GPIO_Pin_4
#define KEY3_PIN    GPIO_Pin_5
#define KEY4_PIN    GPIO_Pin_6

#define KEY_NUM     4U
#define KEY_DEBOUNCE_TIME   2U      /* 连续 2 次一致才认账（20ms × 2 = 40ms） */

static uint8_t s_stable = 0U;               /* bit0~3：消抖后的按下状态 */
static uint8_t s_count[KEY_NUM];            /* 每个键的变化计数 */

/* 读一次引脚，把四个键压成一个位图（1=按下） */
static uint8_t Key_ReadRaw(void)
{
    uint8_t raw = 0U;

    if(GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN) == Bit_SET)    /* PA0 按下为高 */
    {
        raw |= 0x01U;
    }

    if(GPIO_ReadInputDataBit(KEY234_PORT, KEY2_PIN) == Bit_RESET) /* PC4 按下为低 */
    {
        raw |= 0x02U;
    }

    if(GPIO_ReadInputDataBit(KEY234_PORT, KEY3_PIN) == Bit_RESET) /* PC5 按下为低 */
    {
        raw |= 0x04U;
    }

    if(GPIO_ReadInputDataBit(KEY234_PORT, KEY4_PIN) == Bit_RESET) /* PC6 按下为低 */
    {
        raw |= 0x08U;
    }

    return raw;
}

/* PA0 内部下拉，PC4/PC5/PC6 内部上拉，与板上的外部电阻同向，不冲突 */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);

    /* KEY1：PA0 下拉输入 */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Pin   = KEY1_PIN;
    GPIO_Init(KEY1_PORT, &GPIO_InitStructure);

    /* KEY2~KEY4：PC4/PC5/PC6 上拉输入 */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin   = KEY2_PIN | KEY3_PIN | KEY4_PIN;
    GPIO_Init(KEY234_PORT, &GPIO_InitStructure);

    /* 以当前电平作为初始稳定状态，避免上电瞬间误触发 */
    s_stable = Key_ReadRaw();

    s_count[0] = 0U;
    s_count[1] = 0U;
    s_count[2] = 0U;
    s_count[3] = 0U;
}

/*
 * 非阻塞扫描：
 *   每到一次（建议 20ms），比较原始电平和稳定状态；
 *   连续 KEY_DEBOUNCE_TIME 次不一致才更新稳定状态；
 *   只在“由松变按”的那一刻返回键号，按住不会重复返回。
 */
KeyId Key_Scan(void)
{
    uint8_t raw;
    uint8_t i;
    uint8_t mask;

    raw = Key_ReadRaw();

    for(i = 0U; i < KEY_NUM; i++)
    {
        mask = (uint8_t)(1U << i);

        if(((raw & mask) != 0U) != ((s_stable & mask) != 0U))
        {
            s_count[i]++;

            if(s_count[i] >= KEY_DEBOUNCE_TIME)
            {
                s_count[i]  = 0U;
                s_stable   ^= mask;             /* 稳定状态翻转 */

                if((s_stable & mask) != 0U)
                {
                    return (KeyId)(i + 1U);     /* 只上报按下事件 */
                }
            }
        }
        else
        {
            s_count[i] = 0U;                    /* 电平抖回，计数清零 */
        }
    }

    return KEY_NONE;
}

uint8_t Key_IsPressed(KeyId key)
{
    if((key < KEY1) || (key > KEY4))
    {
        return 0U;
    }

    return (uint8_t)((s_stable >> ((uint8_t)key - 1U)) & 0x01U);
}
