#ifndef __HX711_H
#define	__HX711_H

#include "stm32f1xx_hal.h"
#include "gpio.h"
#include "delay.h"
#include "math.h"

// 全局零点变量（空秤原始值，供重量换算使用）
extern int32_t hx711_zero; //测试8477300
// 标定参数：已知重量（g）和对应的传感器原始值（需根据实际硬件校准）
#define CAL_WEIGHT    100.0f  // 100g砝码
//#define CAL_RAW_VALUE 8516670 // 100g砝码对应的传感器原始值
extern uint32_t CAL_RAW_VALUE; // 100g砝码对应的传感器原始值
#define HX711_SCK_H				HAL_GPIO_WritePin(SCK_GPIO_Port,SCK_Pin,GPIO_PIN_SET);
#define HX711_SCK_L				HAL_GPIO_WritePin(SCK_GPIO_Port,SCK_Pin,GPIO_PIN_RESET);
#define HX711_DT				HAL_GPIO_ReadPin(DOUT_GPIO_Port,DOUT_Pin)


unsigned long HX711_GetData(void);

// 零点校准：空秤时调用，获取并保存零点原始值
void HX711_CalZero(void);
// 直接获取实际重量（g），返回浮点型，无需主函数手动换算
float HX711_GetWeight(void);
// 原始数据
uint32_t HX711_ReadRaw(void);
// 滤波版本
float HX711_GetWeight_LPF(void);
#endif /* __ADC_H */


/*
 float weight = 0.0f;
 int32_t reset = HX711_GetData();//8658679
  int value  = HX711_GetData();
  LCD_ShowFloatNum1(45, 100, (float)(value-reset)*100.0f/(float)(8768602-reset), 5, BLUE, WHITE, 16);  
*/

/*
校准版：  hx711_zero = 8477300;
  //float weight = 0.0f;
  //int32_t reset = HX711_GetData();//8658679
//  int value  = HX711_GetData();
//  LCD_ShowFloatNum1(45, 100, (float)(value-reset)*100.0f/(float)(8511607-reset), 5, BLUE, WHITE, 16);    
//LCD_ShowULongNum(45, 100, (u32)HX711_GetData(), 9, BLUE, WHITE, 16);  
  //LCD_ShowFloatNum1(45, 80, HX711_GetWeight(), 6, BLUE, WHITE, 16);  
*/
