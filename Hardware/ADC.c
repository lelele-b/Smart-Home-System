#include "ADC.h"

#define ADC_BUF_LEN     20U         /* DMA 缓冲：2 个通道 x 10 组 */
#define ADC_AVG_TIMES   10U         /* 求平均的组数 */

/* DMA 搬运目标，必须用 volatile 修饰，防止编译器优化掉 */
static volatile uint16_t s_adc_buf[ADC_BUF_LEN];

static uint16_t s_light = 0U;
static uint16_t s_smoke = 0U;

/*
 * 函数：ADC_Init_Multi
 * 功能：初始化 PA5 / PC1 为模拟输入，ADC1 扫描 + 连续转换
 *       + DMA 循环搬运，并完成 ADC 自校准
 */
void ADC_Init_Multi(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    DMA_InitTypeDef  DMA_InitStructure;
    ADC_InitTypeDef  ADC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_ADC1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);           /* 72MHz/6=12MHz，不能超过 14MHz */

    /* 两路模拟输入引脚 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* DMA1 通道 1 服务于 ADC1 */
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)s_adc_buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = ADC_BUF_LEN;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    /* ADC1 工作参数 */
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = ENABLE;              /* 多通道扫描 */
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;              /* 连续转换 */
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 2U;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 通道 5 排第 1、通道 11 排第 2，采样时间给到最长 239.5 周期 */
    ADC_RegularChannelConfig(ADC1, ADC_LIGHT_CH, 1U, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_SMOKE_CH, 2U, ADC_SampleTime_239Cycles5);

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    /* ADC 自校准：必须先复位再启动，否则前几次转换不准 */
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1) == SET)
    {
    }
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1) == SET)
    {
    }

    /* 软件触发一次，之后由连续转换模式自动循环 */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/*
 * 函数：ADC_Update
 * 功能：把 DMA 缓冲区里的 10 组两通道数据分别求平均
 *       结果存到 s_light / s_smoke，主循环每 2 秒调用一次即可
 */
void ADC_Update(void)
{
    uint8_t  i;
    uint32_t sum_light = 0U;
    uint32_t sum_smoke = 0U;

    for(i = 0U; i < ADC_AVG_TIMES; i++)
    {
        sum_light += s_adc_buf[i * 2U];
        sum_smoke += s_adc_buf[i * 2U + 1U];
    }

    s_light = (uint16_t)(sum_light / ADC_AVG_TIMES);
    s_smoke = (uint16_t)(sum_smoke / ADC_AVG_TIMES);
}

/* 函数：ADC_GetLight  功能：取最近一次平均后的光照采样值 */
uint16_t ADC_GetLight(void)
{
    return s_light;
}

/* 函数：ADC_GetSmoke  功能：取最近一次平均后的烟雾采样值 */
uint16_t ADC_GetSmoke(void)
{
    return s_smoke;
}
