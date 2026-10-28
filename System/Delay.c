#include "Delay.h"

/* ==================== SysTick 1ms 计时 ==================== */
static volatile uint32_t s_tick_ms = 0U;

/*
 * 函数：Delay_Init
 * 功能：SysTick 配成 1ms 中断，作为整个系统的节拍源
 * 说明：必须先调用，之后 Delay_ms/Delay_us/Delay_GetTick 才有效
 */
void Delay_Init(void)
{
    /* SysTick：每 1ms 中断一次。SystemCoreClock = 72 000 000，72000 < 0xFFFFFF */
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U)
    {
        while (1)
        {
        }
    }

    /* 让 SysTick 用较低优先级，避免打断 LCD 的 FSMC 时序 */
    NVIC_SetPriority(SysTick_IRQn, 0x0FU);
}

/*
 * 函数：SysTick_Handler
 * 功能：1ms 中断服务函数，累计毫秒计数
 * 说明：stm32f10x_it.c 里的同名函数已注释掉，不会重复定义
 */
void SysTick_Handler(void)
{
    s_tick_ms++;
}

/*
 * 函数：Delay_GetTick
 * 功能：返回上电以来的毫秒数
 * 返回：毫秒计数（约 49 天回绕一次）
 */
uint32_t Delay_GetTick(void)
{
    return s_tick_ms;
}

/*
 * 函数：Delay_ms
 * 参数：ms —— 要延时的毫秒数
 * 功能：毫秒级阻塞延时，靠 SysTick 计数判断，不占用空循环精度
 */
void Delay_ms(uint32_t ms)
{
    uint32_t start = s_tick_ms;

    if (ms == 0U)
    {
        return;
    }

    while ((s_tick_ms - start) < ms)
    {
    }
}

/*
 * 函数：Delay_us
 * 参数：us —— 要延时的微秒数
 * 功能：微秒级阻塞延时
 *
 * 为什么不用 DWT 的 CYCCNT：
 *   本项目 Start\core_cm3.h 是 SPL V3.5.0 附带的老版 CMSIS（没有 DWT 类型），
 *   而且实测这块板子上即使置位 DEMCR.TRCENA 和 DWT_CTRL.CYCCNTENA，
 *   CYCCNT 也不递增（DHT11 读数时卡死在 Delay_us 里）。
 *   所以改成读 SysTick 的当前值寄存器 VAL：它是 24 位向下计数器，
 *   每个内核周期减 1，1us 分辨率足够 DHT11 用。
 *
 * 说明：VAL 每 1ms 被硬件重装一次，所以要用"上次值 + LOAD + 1 - 本次值"
 *       的方式累加，跨过重装点也不会算错。
 */
void Delay_us(uint32_t us)
{
    uint32_t ticks;
    uint32_t last;
    uint32_t now;
    uint32_t elapsed = 0U;
    uint32_t diff;

    if (us == 0U)
    {
        return;
    }

    ticks = us * (SystemCoreClock / 1000000U);

    /* SysTick 还没启动时退化成近似空循环，避免死等 */
    if ((SysTick->CTRL & 0x01U) == 0U)
    {
        volatile uint32_t n = us * 8U;

        while (n != 0U)
        {
            n--;
        }
        return;
    }

    last = SysTick->VAL;
    while (elapsed < ticks)
    {
        now = SysTick->VAL;

        if (now <= last)
        {
            diff = last - now;
        }
        else
        {
            diff = last + SysTick->LOAD + 1U - now;
        }

        elapsed += diff;
        last = now;
    }
}
