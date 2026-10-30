#include "WiFi.h"
#include "Delay.h"
#include "stdio.h"
#include "string.h"

/* ---------------- 硬件定义 ---------------- */
#define WIFI_UART           USART3
#define WIFI_UART_RCC       RCC_APB1Periph_USART3
#define WIFI_GPIO_RCC       (RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOE)

#define WIFI_TX_PORT        GPIOB
#define WIFI_TX_PIN         GPIO_Pin_10      /* USART3_TX */
#define WIFI_RX_PORT        GPIOB
#define WIFI_RX_PIN         GPIO_Pin_11      /* USART3_RX */
#define WIFI_RST_PORT       GPIOE
#define WIFI_RST_PIN        GPIO_Pin_6       /* 模块复位脚，低电平复位 */

#define WIFI_BAUD           115200U

#define WIFI_AT_TIMEOUT     1000U           /* 普通 AT 命令超时 */
#define WIFI_JOIN_TIMEOUT   15000U          /* 连热点超时 */
#define WIFI_TCP_TIMEOUT    8000U           /* 连服务器超时 */

/* 接收缓冲：中断写，主循环读 */
static volatile char     s_rx_buf[WIFI_RX_SIZE + 1U];
static volatile uint16_t s_rx_len   = 0U;
static volatile uint8_t  s_rx_idle  = 0U;   /* 一帧收完（总线空闲）置 1 */

static uint8_t s_connected = 0U;

/*
 * 函数：WIFI_ResetPinInit
 * 功能：PE6 配成推挽输出，默认输出高电平（模块正常工作）
 */
static void WIFI_ResetPinInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin   = WIFI_RST_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(WIFI_RST_PORT, &GPIO_InitStructure);

    GPIO_SetBits(WIFI_RST_PORT, WIFI_RST_PIN);
}

/*
 * 函数：WIFI_UsartInit
 * 功能：USART3 配成 115200 8N1，开接收中断 + 空闲中断
 */
static void WIFI_UsartInit(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(WIFI_GPIO_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(WIFI_UART_RCC, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = WIFI_TX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(WIFI_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = WIFI_RX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(WIFI_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = WIFI_BAUD;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(WIFI_UART, &USART_InitStructure);

    USART_ITConfig(WIFI_UART, USART_IT_RXNE, ENABLE);
    USART_ITConfig(WIFI_UART, USART_IT_IDLE, ENABLE);   /* 总线空闲表示一帧结束 */

    NVIC_InitStructure.NVIC_IRQChannel                   = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1U;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 2U;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(WIFI_UART, ENABLE);
}

/*
 * 函数：USART3_IRQHandler
 * 功能：ESP8266 接收中断：收字节进缓冲，空闲中断置一帧结束标志
 * 说明：读出 DR 才能清掉 IDLE 标志（顺序是先读 SR 再读 DR）
 */
void USART3_IRQHandler(void)
{
    if(USART_GetITStatus(WIFI_UART, USART_IT_RXNE) != RESET)
    {
        char data = (char)USART_ReceiveData(WIFI_UART);

        if(s_rx_len < WIFI_RX_SIZE)
        {
            s_rx_buf[s_rx_len] = data;
            s_rx_len++;
            s_rx_buf[s_rx_len] = '\0';
        }
        USART_ClearITPendingBit(WIFI_UART, USART_IT_RXNE);
    }

    if(USART_GetITStatus(WIFI_UART, USART_IT_IDLE) != RESET)
    {
        (void)USART_ReceiveData(WIFI_UART);             /* 先读 SR 再读 DR 清 IDLE */
        s_rx_idle = 1U;
    }
}

/* 函数：WIFI_ClearRx  功能：清空接收缓冲，准备接下一帧 */
static void WIFI_ClearRx(void)
{
    s_rx_len  = 0U;
    s_rx_idle = 0U;
    s_rx_buf[0] = '\0';
}

/* 函数：WIFI_SendString  功能：往 ESP8266 发一串字符（不自动加回车） */
static void WIFI_SendString(const char *str)
{
    while(*str != '\0')
    {
        while(USART_GetFlagStatus(WIFI_UART, USART_FLAG_TXE) == RESET)
        {
        }
        USART_SendData(WIFI_UART, (uint8_t)(*str));
        str++;
    }
}

/*
 * 函数：WIFI_SendCmd
 * 功能：发一条 AT 命令，自动补 \r\n */
static void WIFI_SendCmd(const char *cmd)
{
    WIFI_SendString(cmd);
    WIFI_SendString("\r\n");
}

/* 函数：WIFI_TokenInRx  功能：缓冲里出现关键字就返回 1（只查一次，不等待） */
static uint8_t WIFI_TokenInRx(const char *token)
{
    return (strstr((const char *)s_rx_buf, token) != 0) ? 1U : 0U;
}

/* 联网状态机的各个步骤 */
typedef enum
{
    WIFI_ST_IDLE = 0U,          /* 空闲 */
    WIFI_ST_RESET_LOW,          /* 把模块复位脚拉低 */
    WIFI_ST_RESET_WAIT,         /* 等模块启动 */
    WIFI_ST_AT,                 /* 试 AT，确认模块和波特率 */
    WIFI_ST_ATE0,               /* 关回显 */
    WIFI_ST_GMR,                /* 读模块版本（只打印） */
    WIFI_ST_CWMODE,             /* Station 模式 */
    WIFI_ST_JOIN,               /* 连 2.4G 热点 */
    WIFI_ST_CIPSTART,           /* 连巴法云 TCP */
    WIFI_ST_CIPMODE,            /* 开透传 */
    WIFI_ST_CIPSEND,            /* 等模块回 ">" */
    WIFI_ST_SUBSCRIBE,          /* 发订阅命令 */
    WIFI_ST_READY,              /* 已连上 */
    WIFI_ST_RETRY               /* 失败后等一会儿自动重来 */
} WiFiState;

static uint8_t  s_state      = (uint8_t)WIFI_ST_IDLE;
static uint32_t s_state_tick = 0U;      /* 进入当前状态的时刻 */
static uint8_t  s_at_try     = 0U;      /* AT 试了几次 */
static uint8_t  s_send_try   = 0U;      /* CIPSEND 试了几次 */
static uint8_t  s_send_phase = 0U;      /* 0=还没发 CIPSEND  1=已发，等 ">" */
static uint8_t  s_last_err   = WIFI_OK;
static char     s_cmd[160];

/* 函数：WIFI_Enter  功能：切到下一个状态并重记时间戳 */
static void WIFI_Enter(uint8_t state)
{
    s_state      = state;
    s_state_tick = Delay_GetTick();
}

/* 函数：WIFI_Elapsed  功能：当前状态已经停留了多少毫秒 */
static uint32_t WIFI_Elapsed(void)
{
    return Delay_GetTick() - s_state_tick;
}

/* 函数：WIFI_Fail  功能：联网失败，记下错误码，十秒后自动重来 */
static void WIFI_Fail(uint8_t err)
{
    s_last_err  = err;
    s_connected = 0U;
    WIFI_Enter((uint8_t)WIFI_ST_RETRY);
}

/*
 * 函数：WiFi_Init
 * 功能：只做硬件初始化（复位脚 + USART3 中断），不联网
 * 说明：联网单独用 WiFi_Connect()，这样开机时屏幕和按键已经能用了，
 *       网络在后台重试，不影响本地显示
 */
/* 函数：WiFi_ConfigReady  功能：热点名称和巴法云 UID 都改成自己的了才返回 1 */
uint8_t WiFi_ConfigReady(void)
{
    if(strncmp(WIFI_SSID, "YOUR_WIFI_NAME", 14) == 0)
    {
        return 0U;                  /* 热点名称还是占位符 */
    }

    if(strncmp(WIFI_BEMFA_UID, "YOUR_BEMFA_UID", 14) == 0)
    {
        return 0U;                  /* 巴法云 UID 还是占位符 */
    }

    return 1U;
}

void WiFi_Init(void)
{
    WIFI_ResetPinInit();
    WIFI_UsartInit();
}

/*
 * 函数：WiFi_ConnectStart
 * 功能：启动一次联网流程，只做状态切换 + 拉低复位脚，立刻返回
 * 说明：真正联网在主循环里由 WiFi_Task() 一步步推进，不再阻塞
 */
void WiFi_ConnectStart(void)
{
    s_connected  = 0U;
    s_at_try     = 0U;
    s_send_try   = 0U;
    s_send_phase = 0U;
    s_last_err   = WIFI_OK;

    WIFI_ClearRx();
    GPIO_ResetBits(WIFI_RST_PORT, WIFI_RST_PIN);
    WIFI_Enter((uint8_t)WIFI_ST_RESET_LOW);
}

/*
 * 函数：WiFi_Task
 * 返回：WIFI_OK 已连上；WIFI_BUSY 还在连；其它值 = 上一次失败的错误码
 * 功能：联网状态机，主循环每 50ms 调一次
 *       等热点最多 15 秒也不卡按键，失败自动重试
 */
uint8_t WiFi_Task(void)
{
    switch(s_state)
    {
        case WIFI_ST_IDLE:
            return (s_connected != 0U) ? WIFI_OK : WIFI_BUSY;

        case WIFI_ST_RESET_LOW:
            if(WIFI_Elapsed() >= 300U)
            {
                GPIO_SetBits(WIFI_RST_PORT, WIFI_RST_PIN);
                WIFI_Enter((uint8_t)WIFI_ST_RESET_WAIT);
            }
            return WIFI_BUSY;

        case WIFI_ST_RESET_WAIT:
            if(WIFI_Elapsed() >= 1200U)
            {
                WIFI_ClearRx();
                WIFI_SendCmd("AT");
                s_at_try = 1U;
                WIFI_Enter((uint8_t)WIFI_ST_AT);
            }
            return WIFI_BUSY;

        case WIFI_ST_AT:
            if(WIFI_TokenInRx("OK") != 0U)
            {
                WIFI_ClearRx();
                WIFI_SendCmd("ATE0");
                WIFI_Enter((uint8_t)WIFI_ST_ATE0);
            }
            else if(WIFI_Elapsed() >= WIFI_AT_TIMEOUT)
            {
                if(s_at_try < 3U)
                {
                    s_at_try++;
                    WIFI_ClearRx();
                    WIFI_SendCmd("AT");
                    WIFI_Enter((uint8_t)WIFI_ST_AT);
                }
                else
                {
                    printf("WIFI: AT no answer\r\n");
                    WIFI_Fail(WIFI_ERR_AT);
                }
            }
            return WIFI_BUSY;

        case WIFI_ST_ATE0:
            if((WIFI_TokenInRx("OK") != 0U) || (WIFI_Elapsed() >= WIFI_AT_TIMEOUT))
            {
                WIFI_ClearRx();
                WIFI_SendCmd("AT+GMR");
                WIFI_Enter((uint8_t)WIFI_ST_GMR);
            }
            return WIFI_BUSY;

        case WIFI_ST_GMR:
            if((WIFI_TokenInRx("OK") != 0U) || (WIFI_Elapsed() >= WIFI_AT_TIMEOUT))
            {
                printf("WIFI GMR: [%s]\r\n", (char *)s_rx_buf);
                WIFI_ClearRx();
                WIFI_SendCmd("AT+CWMODE=1");
                WIFI_Enter((uint8_t)WIFI_ST_CWMODE);
            }
            return WIFI_BUSY;

        case WIFI_ST_CWMODE:
            if(WIFI_TokenInRx("OK") != 0U)
            {
                sprintf(s_cmd, "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_PASSWORD);
                WIFI_ClearRx();
                WIFI_SendCmd(s_cmd);
                WIFI_Enter((uint8_t)WIFI_ST_JOIN);
            }
            else if(WIFI_Elapsed() >= WIFI_AT_TIMEOUT)
            {
                printf("WIFI: set mode fail\r\n");
                WIFI_Fail(WIFI_ERR_AT);
            }
            return WIFI_BUSY;

        case WIFI_ST_JOIN:
            if(WIFI_TokenInRx("OK") != 0U)
            {
                printf("WIFI: join AP ok\r\n");
                sprintf(s_cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", WIFI_SERVER, (int)WIFI_PORT);
                WIFI_ClearRx();
                WIFI_SendCmd(s_cmd);
                WIFI_Enter((uint8_t)WIFI_ST_CIPSTART);
            }
            else if(WIFI_TokenInRx("FAIL") != 0U)
            {
                printf("WIFI: join AP fail (check SSID/PWD/2.4G)\r\n");
                WIFI_Fail(WIFI_ERR_JOIN);
            }
            else if(WIFI_Elapsed() >= WIFI_JOIN_TIMEOUT)
            {
                printf("WIFI: join AP timeout\r\n");
                WIFI_Fail(WIFI_ERR_JOIN);
            }
            return WIFI_BUSY;

        case WIFI_ST_CIPSTART:
            if(WIFI_TokenInRx("CONNECT") != 0U)
            {
                WIFI_ClearRx();
                WIFI_SendCmd("AT+CIPMODE=1");
                WIFI_Enter((uint8_t)WIFI_ST_CIPMODE);
            }
            else if(WIFI_TokenInRx("ERROR") != 0U)
            {
                printf("WIFI: tcp connect fail\r\n");
                WIFI_Fail(WIFI_ERR_TCP);
            }
            else if(WIFI_Elapsed() >= WIFI_TCP_TIMEOUT)
            {
                printf("WIFI: tcp connect timeout\r\n");
                WIFI_Fail(WIFI_ERR_TCP);
            }
            return WIFI_BUSY;

        case WIFI_ST_CIPMODE:
            if(WIFI_TokenInRx("OK") != 0U)
            {
                s_send_phase = 0U;
                WIFI_Enter((uint8_t)WIFI_ST_CIPSEND);
            }
            else if(WIFI_Elapsed() >= WIFI_AT_TIMEOUT)
            {
                printf("WIFI: set transparent fail\r\n");
                WIFI_Fail(WIFI_ERR_SEND);
            }
            return WIFI_BUSY;

        case WIFI_ST_CIPSEND:
            if(s_send_phase == 0U)
            {
                /* AT 1.2.0 固件刚切完 CIPMODE 会回 "busy p..."，先等半秒再要 ">" */
                if(WIFI_Elapsed() >= 500U)
                {
                    WIFI_ClearRx();
                    WIFI_SendCmd("AT+CIPSEND");
                    s_send_phase = 1U;
                    WIFI_Enter((uint8_t)WIFI_ST_CIPSEND);
                }
                return WIFI_BUSY;
            }

            if(WIFI_TokenInRx(">") != 0U)
            {
                s_send_phase = 0U;
                sprintf(s_cmd, "cmd=1&uid=%s&topic=%s\r\n", WIFI_BEMFA_UID, WIFI_TOPIC_SUB);
                WIFI_ClearRx();
                WIFI_SendString(s_cmd);
                WIFI_Enter((uint8_t)WIFI_ST_SUBSCRIBE);
            }
            else if(WIFI_Elapsed() >= 2000U)
            {
                s_send_try++;

                if(s_send_try >= 5U)
                {
                    printf("WIFI: enter send fail\r\n");
                    s_send_phase = 0U;
                    WIFI_Fail(WIFI_ERR_SEND);
                }
                else
                {
                    printf("WIFI: cipsend retry %d\r\n", (int)s_send_try);
                    WIFI_ClearRx();
                    WIFI_SendCmd("AT+CIPSEND");
                    WIFI_Enter((uint8_t)WIFI_ST_CIPSEND);
                }
            }
            return WIFI_BUSY;

        case WIFI_ST_SUBSCRIBE:
            if(WIFI_Elapsed() >= 300U)
            {
                s_connected = 1U;
                printf("WIFI: bemfa ready\r\n");
                WIFI_Enter((uint8_t)WIFI_ST_READY);
            }
            return WIFI_BUSY;

        case WIFI_ST_READY:
            return WIFI_OK;

        case WIFI_ST_RETRY:
            if(WIFI_Elapsed() >= 10000U)
            {
                WiFi_ConnectStart();
                return WIFI_BUSY;
            }
            return s_last_err;

        default:
            WIFI_Enter((uint8_t)WIFI_ST_IDLE);
            return WIFI_BUSY;
    }
}

/* 函数：WiFi_IsConnected  功能：1 = 已经连上巴法云并进入透传 */
uint8_t WiFi_IsConnected(void)
{
    return s_connected;
}

/*
 * 函数：WiFi_PublishData
 * 功能：按固定顺序把 8 项数据打包发给巴法云（透传模式下直接写串口）
 * 说明：格式 #温度#湿度#光照#烟雾#led#beep#rgb#relay#
 *       小程序端用 split("#") 取值，索引 1~8
 */
void WiFi_PublishData(int16_t tem_x10, uint8_t hum, uint16_t light, uint16_t smoke,
                      uint8_t led, uint8_t beep, uint8_t rgb, uint8_t relay)
{
    char buf[128];

    if(s_connected == 0U)
    {
        return;
    }

    sprintf(buf, "cmd=2&uid=%s&topic=%s&msg=#%d.%d#%d#%d#%d#%d#%d#%d#%d#\r\n",
            WIFI_BEMFA_UID,
            WIFI_TOPIC_PUB,
            (int)(tem_x10 / 10), (int)(tem_x10 % 10),
            (int)hum,
            (int)light,
            (int)smoke,
            (int)led, (int)beep, (int)rgb, (int)relay);

    WIFI_SendString(buf);

    /* 把实际发出去的报文打到调试串口，方便和云端 / 小程序对照 */
    printf("WIFI TX: %s", buf);
}

/*
 * 函数：WIFI_FindValue
 * 参数：buf 收到的整帧，key 关键字（例如 "led"）
 * 返回：1 找到了且值是数字，0 没找到
 * 功能：在 "led:1" 这种片段里取出冒号后面的数字
 */
static uint8_t WIFI_FindValue(const char *buf, const char *key, uint8_t *value)
{
    const char *p = strstr(buf, key);

    if(p == 0)
    {
        return 0U;
    }

    p = strchr(p, ':');                 /* 找到冒号 */
    if(p == 0)
    {
        return 0U;
    }

    p++;                                /* 跳过冒号 */
    if((*p < '0') || (*p > '9'))
    {
        return 0U;
    }

    *value = (uint8_t)(*p - '0');

    return 1U;
}

/*
 * 函数：WiFi_GetControl
 * 参数：led / beep / rgb / relay —— 输出参数，调用者传地址进来
 * 返回：按位组合的 WIFI_CTRL_xxx，0 表示这一帧里没有控制命令
 * 功能：解析巴法云下发的 devicectrl 消息，例如
 *       cmd=1&uid=xxx&topic=devicectrl006&msg=led:1
 *       cmd=1&uid=xxx&topic=devicectrl006&msg=rgb:2
 */
uint8_t WiFi_GetControl(uint8_t *led, uint8_t *beep, uint8_t *rgb, uint8_t *relay)
{
    uint8_t mask = 0U;
    uint8_t val;

    if(s_rx_idle == 0U)
    {
        return 0U;
    }

    s_rx_idle = 0U;                     /* 这一帧只看一次 */

    /* 不是控制主题的消息（例如自己发出去的 sensordata 回显）直接忽略 */
    if(strstr((const char *)s_rx_buf, WIFI_TOPIC_PUB) != 0)
    {
        if(strstr((const char *)s_rx_buf, "devicectrl") == 0)
        {
            WIFI_ClearRx();
            return 0U;
        }
    }

    if(WIFI_FindValue((const char *)s_rx_buf, "led", &val) != 0U)
    {
        *led = val;
        mask |= WIFI_CTRL_LED;
    }
    if(WIFI_FindValue((const char *)s_rx_buf, "beep", &val) != 0U)
    {
        *beep = val;
        mask |= WIFI_CTRL_BEEP;
    }
    if(WIFI_FindValue((const char *)s_rx_buf, "rgb", &val) != 0U)
    {
        *rgb = val;
        mask |= WIFI_CTRL_RGB;
    }
    if(WIFI_FindValue((const char *)s_rx_buf, "relay", &val) != 0U)
    {
        *relay = val;
        mask |= WIFI_CTRL_RELAY;
    }

    WIFI_ClearRx();

    return mask;
}

