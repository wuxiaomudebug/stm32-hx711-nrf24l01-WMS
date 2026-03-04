#include "delay.h"


/**
  * @brief  微秒级延时（基于SysTick）
  * @param  us: 延时时间，单位：微秒（范围：0 ~ 1000000，即最大1秒）
  * @retval 无
  * @note   1. 依赖SysTick时钟（默认与系统时钟一致，如72MHz/168MHz）
  *         2. 若us=0，直接返回（避免无效操作）
  *         3. 延时精度误差通常<1%，满足I2C/SPI等外设时序需求
  */
void HAL_Delay_us(uint32_t us)
{
    if (us == 0)
        return;

    uint32_t ticks;          // 需要的SysTick计数值
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;  // SysTick最大计数值（由HAL_Init()初始化）
    uint32_t freq = SystemCoreClock / 1000000;  // SysTick频率（MHz，1us对应计数值）

    ticks = us * freq;       // 计算需要的总计数值（1us = freq个SysTick时钟周期）

    told = SysTick->VAL;     // 记录初始计数值
    while (1)
    {
        tnow = SysTick->VAL; // 读取当前计数值
        if (tnow != told)
        {
            // SysTick是递减计数器，需判断是否溢出（VAL从0→reload时溢出）
            if (tnow < told)
                tcnt += told - tnow;  // 未溢出：累加已过去的计数值
            else
                tcnt += reload - tnow + told;  // 溢出：累加溢出部分+剩余部分

            told = tnow;

            // 若累计计数值达到目标，退出循环
            if (tcnt >= ticks)
                break;
        }
    }
}

void Delay_ms(uint32_t ms){
HAL_Delay(ms);

}

void delay_us(uint32_t us)
{
    if (us == 0)
        return;

    uint32_t ticks;          // 需要的SysTick计数值
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;  // SysTick最大计数值（由HAL_Init()初始化）
    uint32_t freq = SystemCoreClock / 1000000;  // SysTick频率（MHz，1us对应计数值）

    ticks = us * freq;       // 计算需要的总计数值（1us = freq个SysTick时钟周期）

    told = SysTick->VAL;     // 记录初始计数值
    while (1)
    {
        tnow = SysTick->VAL; // 读取当前计数值
        if (tnow != told)
        {
            // SysTick是递减计数器，需判断是否溢出（VAL从0→reload时溢出）
            if (tnow < told)
                tcnt += told - tnow;  // 未溢出：累加已过去的计数值
            else
                tcnt += reload - tnow + told;  // 溢出：累加溢出部分+剩余部分

            told = tnow;

            // 若累计计数值达到目标，退出循环
            if (tcnt >= ticks)
                break;
        }
    }
}
