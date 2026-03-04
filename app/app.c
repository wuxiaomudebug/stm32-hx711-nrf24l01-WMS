#include "app.h"

#include "serial.h"

#include "lcd.h"
#include "hx711.h"
#include "nrf24l01.h"

uint8_t bin_id = 1;  // 当前仓位编号

//全局变量/参数
uint16_t page = 0;//显示页面0
bool lcd_flag = false;
float weight = 0;//重量
uint32_t pcs = 0;//数量
uint32_t itemMass = 1;//单个物品质量
uint32_t item_num = 0;//物品数量

//ntf24l01
const uint8_t NRF24_ADDR[5] = {0x11, 0x22, 0x33, 0x44, 0x55};// 全局/局部定义：32字节缓冲区+通信地址（收发端一致）
uint8_t nrf_tx_buf[32] = {0};
bool nrf_flag = false;

//num
uint32_t last_item = 0;
bool isNrf = false;
bool is_num = false;//是否开始计数
bool num_flag = false;//计数规则刷新



void app(void){

  //lcd 160*128横向显示
  LCD_Init();
  LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);

  LCD_Fill(10, 10, 20, 20, RED);    // 绘制红色小方块
  LCD_Fill(10+10, 10, 20+10, 20, GREEN); // 绘制绿色小方块
  LCD_Fill(20+10, 10, 20+10+10, 20, BLUE); // 绘制蓝色小方块

  LCD_DrawLine(0, 0, LCD_W-1, LCD_H-1, BLACK); // 绘制主对角线
  LCD_DrawLine(LCD_W-1, 0, 0, LCD_H-1, RED);    // 绘制副对角线

  LCD_ShowString(10, 20, "Hello World!", BLACK, WHITE, 16, 0); // 16号字体显示英文字符串
  
  LCD_ShowIntNum(65, 0, 1234, 4, RED, WHITE, 16);           // 显示整数1234，占4位宽度
  
  LCD_ShowChinese(10, 60, "库存", BLACK, WHITE, 16, 0);
  LCD_ShowIntNum(45, 60, 0, 4, BLUE, WHITE, 16);
  LCD_ShowString(80, 60, "pcs", BLACK, WHITE, 16, 0);
  LCD_ShowChinese(10, 80, "质量", BLACK, WHITE, 16, 0);
  LCD_ShowIntNum(45, 80, 0, 4, BLUE, WHITE, 16);
  
  
  //HX711
  HX711_CalZero();//零点校准
  //hx711_zero = 8478300;
  
  
  //ntf24l01
  if(NRF24_Init(NRF24_MODE_TX, NRF24_ADDR) != 0)
  {
      // 初始化失败，串口打印提示+死循环（或做LCD报错显示）
      printf("NRF24L01: INIT FAILED! Check hardware/params.\r\n");
      while(1); // 初始化失败，停止后续操作
  }
  else
  {
      printf("NRF24L01: INIT SUCCESS (TX MODE).\r\n");
  }
  
  while(1){

//lCD
if(lcd_flag == true){
  lcd_flag = false;
  //质量
  uint32_t raw_data = HX711_ReadRaw();
  LCD_ShowULongNum(45, 36, raw_data, 9, BLUE, WHITE, 16);
  
  
  weight = HX711_GetWeight_LPF();
  item_num  = (weight / itemMass) + 0.2f;
  
  printf("weight:%.2f item:%lu \r\n",weight, item_num);
  
  if(isNrf == true){
  LCD_ShowIntNum(45, 60, item_num, 4, BLUE, WHITE, 16);
  LCD_ShowString(80, 60, "pcs", BLACK, WHITE, 16, 0);
}

  //if(isNrf == true){
  LCD_ShowIntNum(45, 80, weight, 5, BLUE, WHITE, 16);  
  LCD_ShowString(85, 80, "g", BLACK, WHITE, 16, 0);  
//}

}


//NUM_FALG
if(num_flag == true){
  num_flag = false;
  static uint16_t i_num = 0;//停止计数
  if(item_num != last_item){
    last_item = item_num;
    is_num = true;
    i_num = 0;//每次变化从头计时
  }
  if(is_num == true){
   i_num++;
  }
  if(i_num >= 2){//计数两次ms没有变化
    is_num = false;
    i_num = 0;
    isNrf = true;//发送稳定数据
  }
  
}

//NTF24L01
if(nrf_flag == true){
  nrf_flag = false;
if(isNrf == true){
isNrf = false;
  memset(nrf_tx_buf,0,32);
  //NRF24L01 发送数据
  sprintf((char*)nrf_tx_buf,"id:%u,weight:%.2f,item:%u",bin_id,weight, item_num);
  uint8_t tx_status = NRF24_TxPacket(nrf_tx_buf);
  if(tx_status == 0){
    printf("NRF24L01:TX SUCCESS. data:weight:%.2f \r\n",weight);
    sprintf((char*)nrf_tx_buf,"TX SUCCESS w:%.2f,i:%lu",weight, item_num);
    LCD_ShowString(10, 100, nrf_tx_buf, BLACK, WHITE, 16, 0);
  }else{
    printf("NRF24L01:TX ERROR! data:weight:%.2f \r\n",weight);
    sprintf((char*)nrf_tx_buf,"TX ERROR w:%.2f,i:%lu",weight, item_num);
    LCD_ShowString(10, 100, nrf_tx_buf, BLACK, WHITE, 16, 0);
  }
  
  }
}

// whileEND
    
  }
}

void Ticks(void){
static uint32_t ticks = 0;
if(++ticks >= 60000) ticks = 0;//每分钟清除ticks计数
if(ticks % 100 == 0) lcd_flag = true; //100ms lcd刷新
if(ticks % 300 == 0) num_flag = true; //300ms 计数规则刷新
if(ticks % 500 == 0) nrf_flag = true; //500ms ntf_tx



}

//KEY
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
switch(GPIO_Pin){
case GPIO_PIN_3://KEY1
  LED_TOG();
  itemMass = HX711_GetWeight();//单个物品质量校准
  break;
case GPIO_PIN_4://零点设置
  HX711_CalZero();//零点校准
  break;
case GPIO_PIN_5://100g校准
  CAL_RAW_VALUE = HX711_ReadRaw();
  break;
}

}


