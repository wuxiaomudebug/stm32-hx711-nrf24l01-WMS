#include "hx711.h"

uint32_t CAL_RAW_VALUE = 8516370; // 100g砝码对应的传感器原始值

unsigned long HX711_GetData(void)
{
		unsigned long Count;
		unsigned char i;
		HX711_SCK_L;
	  delay_us(1);
		Count=0;
		while(HX711_DT);
		for (i=0;i<24;i++)
	 {
				HX711_SCK_H;
				delay_us(1);
				Count=Count<<1;
				HX711_SCK_L;
				delay_us(1);
				if(HX711_DT) Count++;
		}
		HX711_SCK_H;
		delay_us(1);
		Count=Count^0x800000;//最高位取反，其他位不变
	                      //在HX71芯片中，count是一个32位的有符号整数，用于存储称重传感器的读数。
	                      //当count的最高位为1时，表示读数为负数，而HX711芯片不支持负数的读数。
	                      //因此，为了将负数转换为正数，需要将count的最高位取反，即将count与0x800000进行异或操作。
                          //具体来说，0x800000的二进制表示为100000000000000000000000，与count进行异或操作后，
	                      //可以将count的最高位从1变为0，从而得到对应的正数读数。
		HX711_SCK_L;
		delay_us(1);
		
		return(Count);
}


// 全局零点变量（空秤原始值，供HX711_GetWeight调用）
int32_t hx711_zero = 0;

// 底层函数：读取HX711原始24位数据
uint32_t HX711_ReadRaw(void)
{
    uint32_t count = 0;
    uint8_t i = 0;
    
    HX711_SCK_L;
    delay_us(1);
    // 等待传感器数据准备完成
    while(HX711_DT);
    // 读取24位原始数据
    for(i = 0; i < 24; i++)
    {
        HX711_SCK_H;
        delay_us(1);
        count = count << 1;  // 左移1位，准备接收下一位
        HX711_SCK_L;
        delay_us(1);
        if(HX711_DT) count++; // 数据为1时，最低位补1
    }
    // 第25个时钟脉冲：选择通道A，增益128（HX711默认配置，适配称重传感器）
    HX711_SCK_H;
    delay_us(1);
    count ^= 0x800000;  // 最高位取反，转换为实际有效数值（HX711芯片协议要求）
    HX711_SCK_L;
    delay_us(1);
    
    return count;
}

// 零点校准函数：空秤（无任何重物）时调用，保存零点原始值
void HX711_CalZero(void)
{
    hx711_zero = HX711_ReadRaw();  // 读取空秤原始值，作为零点
}

// 核心函数：直接返回实际重量（g），无需主函数手动换算
float HX711_GetWeight(void)
{
    int32_t raw_data = 0;
    float real_weight = 0.0f;
    
    raw_data = HX711_ReadRaw();  // 读取传感器当前原始值
    // 重量换算核心公式（原逻辑封装到此处）
    // 公式：实际重量 = (当前原始值-零点值) × 标定重量 / (标定原始值-零点值)
    real_weight = (float)(raw_data - hx711_zero) * CAL_WEIGHT / (float)(CAL_RAW_VALUE - hx711_zero);
    
    // 过滤负重量（空秤时可能因抖动出现微小负数，直接置0）
    if(real_weight < 0.0f)
    {
        real_weight = 0.0f;
    }
    
    return real_weight;  // 直接返回浮点型重量（g）
}


/********************* 一阶RC低通滤波参数配置 *********************/
#define LP_FILTER_FACTOR  0.4f  // 低通滤波系数（建议0.1~0.5）
                                // 0.1：滤波强，响应慢；0.5：滤波弱，响应快；0.2为称重通用值
static float last_filter_weight = 0.0f;  // 静态变量：保存上一次滤波结果，仅本文件可见


/**
 * @brief  低通滤波版HX711获取重量（g）- 嵌套调用原始HX711_GetWeight
 * @note   基于一阶RC低通滤波，专为称重传感器设计，抑制高频抖动，保留重量缓变特性
 *         首次调用时，直接以原始重量作为初始滤波值，无启动延迟
 * @param  无
 * @retval 低通滤波后的浮点型重量值，单位：g
 */
float HX711_GetWeight_LPF(void)
{
    // 1. 嵌套调用原始核心函数，获取单次未滤波重量（复用原有的零点校准、重量换算、负重量过滤逻辑）
    float raw_weight = HX711_GetWeight();
    
    // 2. 一阶RC低通滤波核心计算（首次调用时last_filter_weight=0，直接赋值原始值，无启动偏差）
    last_filter_weight = raw_weight * LP_FILTER_FACTOR + last_filter_weight * (1.0f - LP_FILTER_FACTOR);
    
    // 3. 最终防护：确保滤波后重量非负（极端抖动场景二次防护，与原函数逻辑一致）
    if (last_filter_weight < 0.0f)
    {
        last_filter_weight = 0.0f;
    }
    
    // 4. 返回滤波后稳定重量值
    return last_filter_weight;
}