#include "DHT.h"
#include "Delay.h"

/* ---------------- 硬件定义 ---------------- */
#define DHT_PORT        GPIOG
#define DHT_PIN         GPIO_Pin_11
#define DHT_RCC_PORT    RCC_APB2Periph_GPIOG

#define DHT_TIMEOUT_US      120U        /* 单个电平最长等待 120us，正常不超过 90us */
#define DHT_ONE_MIN_US      40U         /* 高电平超过 40us 判为 1 */
#define DHT_US_PER_CYCLE    (SystemCoreClock / 1000000U)

#define DHT_TIMEOUT_VAL     0xFFFFFFFFUL

/*
 * 函数：DHT_SetOutput
 * 功能：把数据脚切换成推挽输出，用于主机发起始信号
 */
static void DHT_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin   = DHT_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT_PORT, &GPIO_InitStructure);
}

/*
 * 函数：DHT_SetInput
 * 功能：把数据脚切换成浮空输入，由模块的上拉电阻拉高，主机只读
 */
static void DHT_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin   = DHT_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT_PORT, &GPIO_InitStructure);
}

/*
 * 函数：DHT_WaitChange
 * 参数：level 当前电平，timeout_us 超时时间（微秒）
 * 返回：该电平持续的时间（微秒）；超时返回 DHT_TIMEOUT_VAL
 * 功能：等待数据脚从 level 翻转，同时用 SysTick 的 24 位向下计数器
 *       精确测量这个电平持续了多久，用来区分数据位 0 和 1
 *
 * 说明：SysTick->VAL 每 1ms 被硬件重装一次，所以每次差值都要考虑
 *       是否跨过了重装点（本次读数 bigger 说明刚好重装过）
 */
static uint32_t DHT_WaitChange(uint8_t level, uint32_t timeout_us)
{
    uint32_t last = SysTick->VAL;
    uint32_t now;
    uint32_t diff;
    uint32_t elapsed = 0U;
    uint32_t limit   = timeout_us * DHT_US_PER_CYCLE;

    while(GPIO_ReadInputDataBit(DHT_PORT, DHT_PIN) == level)
    {
        now = SysTick->VAL;

        if(now <= last)
        {
            diff = last - now;
        }
        else
        {
            diff = last + SysTick->LOAD + 1U - now;
        }

        elapsed += diff;
        last = now;

        if(elapsed > limit)
        {
            return DHT_TIMEOUT_VAL;
        }
    }

    return elapsed / DHT_US_PER_CYCLE;
}

/*
 * 函数：DHT_Init
 * 功能：把 PG11 配成推挽输出并释放总线（空闲时为高电平）
 */
void DHT_Init(void)
{
    RCC_APB2PeriphClockCmd(DHT_RCC_PORT, ENABLE);

    DHT_SetOutput();
    GPIO_SetBits(DHT_PORT, DHT_PIN);
}

/*
 * 函数：DHT_Read
 * 参数：out 读到的温湿度（不能为空指针）
 * 返回：DHT_OK / DHT_ERR_TIMEOUT / DHT_ERR_CHECK
 * 功能：完整读一次 40 位数据，整次读取约 25ms
 */
uint8_t DHT_Read(DhtData *out)
{
    uint8_t  i;
    uint8_t  j;
    uint8_t  buf[5] = {0U, 0U, 0U, 0U, 0U};
    uint8_t  ret = DHT_OK;
    uint32_t high_us;

    if(out == 0)
    {
        return DHT_ERR_TIMEOUT;
    }

    /* 1. 主机发起始信号：拉低 20ms 后释放，再等 30us 交给模块拉低应答 */
    DHT_SetOutput();
    GPIO_ResetBits(DHT_PORT, DHT_PIN);
    Delay_ms(20U);
    GPIO_SetBits(DHT_PORT, DHT_PIN);
    Delay_us(30U);
    DHT_SetInput();

    /* 2. 关掉 1ms 节拍中断（只清 TICKINT，不动 ENABLE），
     *    否则 1ms 一次的中断会把某一位的高电平拉长，读出来就是乱码。
     *    注意不能用 NVIC_DisableIRQ(SysTick_IRQn)：SysTick 的中断号是 -1，
     *    老版 CMSIS 里这个宏不做负数保护，会访问越界的 NVIC 寄存器直接死机 */
    SysTick->CTRL &= ~(1UL << 1);

    /* 3. 传感器应答：拉低约 80us -> 拉高约 80us -> 再拉低 50us 后开始第 1 位 */
    if(DHT_WaitChange(0U, DHT_TIMEOUT_US) == DHT_TIMEOUT_VAL)
    {
        ret = DHT_ERR_TIMEOUT;
        goto exit_read;
    }
    if(DHT_WaitChange(1U, DHT_TIMEOUT_US) == DHT_TIMEOUT_VAL)
    {
        ret = DHT_ERR_TIMEOUT;
        goto exit_read;
    }

    /* 4. 收 40 位：每位先低 50us，再高 26~28us（数据 0）或 70us（数据 1） */
    for(i = 0U; i < 5U; i++)
    {
        for(j = 0U; j < 8U; j++)
        {
            /* 先等这 50us 的低电平结束 */
            if(DHT_WaitChange(0U, DHT_TIMEOUT_US) == DHT_TIMEOUT_VAL)
            {
                ret = DHT_ERR_TIMEOUT;
                goto exit_read;
            }

            /* 再量高电平有多宽 */
            high_us = DHT_WaitChange(1U, DHT_TIMEOUT_US);
            if(high_us == DHT_TIMEOUT_VAL)
            {
                ret = DHT_ERR_TIMEOUT;
                goto exit_read;
            }

            buf[i] <<= 1;
            if(high_us > DHT_ONE_MIN_US)
            {
                buf[i] |= 0x01U;
            }
        }
    }

    /* 5. 校验：前 4 字节之和的低 8 位应等于第 5 字节 */
    if((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
    {
        ret = DHT_ERR_CHECK;
        goto exit_read;
    }

    out->hum     = buf[0];
    out->tem_x10 = (int16_t)(buf[2] * 10 + (buf[3] & 0x0FU));

exit_read:
    SysTick->CTRL |= (1UL << 1);        /* 恢复 1ms 节拍中断 */
    DHT_SetOutput();
    GPIO_SetBits(DHT_PORT, DHT_PIN);

    return ret;
}
