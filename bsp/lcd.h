#ifndef __LCD_H
#define __LCD_H

#include "main.h"
#include "spi.h"

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define USE_HORIZONTAL 3  //设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏

#if USE_HORIZONTAL==0||USE_HORIZONTAL==1
#define LCD_W 128
#define LCD_H 160
#else
#define LCD_W 160
#define LCD_H 128
#endif

//-----------------LCD端口定义（仅保留控制引脚，SPI使用硬件）---------------- 
// SPI1硬件通信，不再需要模拟SCLK/MOSI引脚
//#define LCD_SCLK_Clr() GPIO_ResetBits(GPIOA,GPIO_Pin_0)//SCL=SCLK
//#define LCD_SCLK_Set() GPIO_SetBits(GPIOA,GPIO_Pin_0)
//#define LCD_MOSI_Clr() GPIO_ResetBits(GPIOA,GPIO_Pin_1)//SDA=MOSI
//#define LCD_MOSI_Set() GPIO_SetBits(GPIOA,GPIO_Pin_1)

#define LCD_RES_Clr()  HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8,GPIO_PIN_RESET)//RES
#define LCD_RES_Set()  HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8,GPIO_PIN_SET)

#define LCD_DC_Clr()   HAL_GPIO_WritePin(GPIOA,GPIO_PIN_12,GPIO_PIN_RESET)//DC
#define LCD_DC_Set()   HAL_GPIO_WritePin(GPIOA,GPIO_PIN_12,GPIO_PIN_SET)
		     
#define LCD_CS_Clr()   HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_RESET)//CS
#define LCD_CS_Set()   HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_SET)

#define LCD_BLK_Clr()  //HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_RESET)//BLK
#define LCD_BLK_Set()  //HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_SET)

#define u8 uint8_t
#define u16 uint16_t

// 声明SPI句柄（需要在main.c中初始化hspi1）
//extern SPI_HandleTypeDef hspi2;
#define SPI_PORT hspi2


//-----------------LCD基础函数---------------- 
void LCD_GPIO_Init(void);//初始化GPIO
void LCD_Writ_Bus(u8 dat);//硬件SPI时序
void LCD_WR_DATA8(u8 dat);//写入一个字节
void LCD_WR_DATA(u16 dat);//写入两个字节
void LCD_WR_REG(u8 dat);//写入一个指令
void LCD_Address_Set(u16 x1,u16 y1,u16 x2,u16 y2);//设置坐标函数
void LCD_Init(void);//LCD初始化


//-----------------LCD函数---------------- 
void LCD_Fill(u16 xsta,u16 ysta,u16 xend,u16 yend,u16 color);//指定区域填充颜色
void LCD_DrawPoint(u16 x,u16 y,u16 color);//在指定位置画一个点
void LCD_DrawLine(u16 x1,u16 y1,u16 x2,u16 y2,u16 color);//在指定位置画一条线
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2,u16 color);//在指定位置画一个矩形
void Draw_Circle(u16 x0,u16 y0,u8 r,u16 color);//在指定位置画一个圆

void LCD_ShowChinese(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);//显示汉字串
void LCD_ShowChinese12x12(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);//显示单个12x12汉字
void LCD_ShowChinese16x16(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);//显示单个16x16汉字
void LCD_ShowChinese24x24(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);//显示单个24x24汉字
void LCD_ShowChinese32x32(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);//显示单个32x32汉字

void LCD_ShowChar(u16 x,u16 y,u8 num,u16 fc,u16 bc,u8 sizey,u8 mode);//显示一个字符
void LCD_ShowString(u16 x,u16 y,const u8 *p,u16 fc,u16 bc,u8 sizey,u8 mode);//显示字符串
u32 mypow(u8 m,u8 n);//求幂
void LCD_ShowIntNum(u16 x,u16 y,u16 num,u8 len,u16 fc,u16 bc,u8 sizey);//显示整数变量
void LCD_ShowULongNum(u16 x,u16 y,unsigned long num,u8 len,u16 fc,u16 bc,u8 sizey);
void LCD_ShowFloatNum1(u16 x,u16 y,float num,u8 len,u16 fc,u16 bc,u8 sizey);//显示两位小数变量

void LCD_ShowPicture(u16 x,u16 y,u16 length,u16 width,const u8 pic[]);//显示图片
void LCD_ShowMixString(u16 x, u16 y, const u8 *str, u16 fc, u16 bc, u8 sizey, u8 mode);//混合显示

//画笔颜色
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE           	 0x001F  
#define BRED             0XF81F
#define GRED 			       0XFFE0
#define GBLUE			       0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			     0XBC40 //棕色
#define BRRED 			     0XFC07 //棕红色
#define GRAY  			     0X8430 //灰色
#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色  
#define GRAYBLUE       	 0X5458 //灰蓝色
#define LIGHTGREEN     	 0X841F //浅绿色
#define LGRAY 			     0XC618 //浅灰色(PANNEL),窗体背景色
#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)

#endif


/*
  LCD_Init();
  LCD_Fill(0,0,LCD_W,LCD_H,WHITE);
    // 1. 清屏（白色背景）
  
  
  // 2. 填充颜色块
  LCD_Fill(10, 10, 50, 50, RED);    // 红色方块
  LCD_Fill(70, 10, 110, 50, GREEN); // 绿色方块
  LCD_Fill(130, 10, 150, 50, BLUE); // 蓝色方块
  HAL_Delay(1000);
  
  // 3. 清屏后绘制基本图形
  LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);
  
  // 画直线
  LCD_DrawLine(0, 0, LCD_W-1, LCD_H-1, YELLOW); // 对角线（黄色）
  LCD_DrawLine(LCD_W-1, 0, 0, LCD_H-1, RED); // 反对角线（紫色）
  HAL_Delay(1000);
  
  // 画矩形
  LCD_DrawRectangle(20, 70, 80, 110, CYAN); // 青色矩形
  HAL_Delay(500);
  
  // 画圆
  Draw_Circle(120, 90, 20, BLACK); // 黑色圆形
  HAL_Delay(500);
  
  // 4. 显示字符串
  LCD_ShowString(10, 120, "Hello LCD!", BLACK, WHITE, 16, 0); // 16号字体，黑色字白色背景
  HAL_Delay(1000);
  
  // 5. 显示数字
  LCD_ShowIntNum(10, 140, 1234, 4, RED, WHITE, 16); // 显示整数1234，4位，红色
  HAL_Delay(1000);
  
  // 6. 显示小数
  LCD_ShowFloatNum1(80, 140, 3.14, 4, BLUE, WHITE, 16); // 显示3.14，4位（含小数点），蓝色
  HAL_Delay(1000);
  
  // 7. 显示汉字（需确保lcdfont.h中有对应字库）
  // 示例：显示"测试"两个字，24号字体，绿色
  LCD_ShowChinese(10, 80, (u8*)"中景", GREEN, WHITE, 24, 0);

*/


