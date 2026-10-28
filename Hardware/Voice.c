#include "Voice.h"

/* ---------------- 硬件定义 ---------------- */
#define VOICE_UART          UART5
#define VOICE_UART_RCC      RCC_APB1Periph_UART5
#define VOICE_GPIO_RCC      (RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD)

#define VOICE_TX_PORT       GPIOC
#define VOICE_TX_PIN        GPIO_Pin_12      /* UART5_TX */
#define VOICE_RX_PORT       GPIOD
#define VOICE_RX_PIN        GPIO_Pin_2       /* UART5_RX */

#define VOICE_BAUD          9600U
#define VOICE_FIFO_SIZE     32U              /* 环形缓冲，2 的整数次幂 */

/* 中断里写入、主循环里读出的环形缓冲 */
static volatile uint8_t  s_fifo[VOICE_FIFO_SIZE];
static volatile uint8_t  s_fifo_head = 0U;   /* 中断写位置 */
static volatile uint8_t  s_fifo_tail = 0U;   /* 主循环读位置 */
static volatile uint16_t s_rx_count  = 0U;   /* 累计收到的字节数，调试用 */

/*
 * 函数：Voice_Init
 * 功能：UART5 配成 9600 8N1，开接收中断
 * 说明：UART5 只能用 PC12 做 TX、PD2 做 RX，引脚是固定的
 */
void Voice_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(VOICE_GPIO_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(VOICE_UART_RCC, ENABLE);

    /* TX：复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = VOICE_TX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(VOICE_TX_PORT, &GPIO_InitStructure);

    /* RX：浮空输入（模块的 TX 是推挽输出，不需要上拉） */
    GPIO_InitStructure.GPIO_Pin   = VOICE_RX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(VOICE_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = VOICE_BAUD;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(VOICE_UART, &USART_InitStructure);

    USART_ITConfig(VOICE_UART, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = UART5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1U;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1U;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(VOICE_UART, ENABLE);
}

/*
 * 函数：UART5_IRQHandler
 * 功能：语音模块接收中断，把字节塞进环形缓冲
 * 说明：一次把 RXNE 的数据读完就退出，不做任何耗时处理
 */
void UART5_IRQHandler(void)
{
    uint8_t data;
    uint8_t next;

    if(USART_GetITStatus(VOICE_UART, USART_IT_RXNE) != RESET)
    {
        data = (uint8_t)USART_ReceiveData(VOICE_UART);
        USART_ClearITPendingBit(VOICE_UART, USART_IT_RXNE);

        s_rx_count++;

        next = (uint8_t)((s_fifo_head + 1U) % VOICE_FIFO_SIZE);
        if(next != s_fifo_tail)                 /* 没满才写，满了丢新数据 */
        {
            s_fifo[s_fifo_head] = data;
            s_fifo_head = next;
        }
    }
}

/*
 * 函数：Voice_Poll
 * 返回：识别到的命令，没有新命令时返回 VOICE_NONE
 * 功能：从环形缓冲里取一个字节并翻译成命令，主循环每 100ms 调一次
 */
VoiceCmd Voice_Poll(void)
{
    uint8_t code;

    if(s_fifo_tail == s_fifo_head)
    {
        return VOICE_NONE;
    }

    code = s_fifo[s_fifo_tail];
    s_fifo_tail = (uint8_t)((s_fifo_tail + 1U) % VOICE_FIFO_SIZE);

    switch(code)
    {
        case VOICE_CODE_TEM:        return VOICE_TEM;
        case VOICE_CODE_HUM:        return VOICE_HUM;
        case VOICE_CODE_LIGHT:      return VOICE_LIGHT;
        case VOICE_CODE_SMOKE:      return VOICE_SMOKE;
        case VOICE_CODE_LED_ON:     return VOICE_LED_ON;
        case VOICE_CODE_LED_OFF:    return VOICE_LED_OFF;
        case VOICE_CODE_RED:        return VOICE_RED;
        case VOICE_CODE_GREEN:      return VOICE_GREEN;
        case VOICE_CODE_BLUE:       return VOICE_BLUE;
        case VOICE_CODE_RELAY_ON:   return VOICE_RELAY_ON;
        case VOICE_CODE_RELAY_OFF:  return VOICE_RELAY_OFF;
        case VOICE_CODE_BEEP_ON:    return VOICE_BEEP_ON;
        case VOICE_CODE_BEEP_OFF:   return VOICE_BEEP_OFF;
        default:                    return VOICE_NONE;
    }
}

/* 函数：Voice_GetRxCount  功能：取累计收到的字节数（调试用） */
uint16_t Voice_GetRxCount(void)
{
    return s_rx_count;
}
