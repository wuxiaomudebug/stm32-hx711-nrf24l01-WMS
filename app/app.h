#ifndef __APP_H__
#define __APP_H__

#include "main.h"

#include "dma.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

#include "stdio.h"
#include "stdbool.h"

void app(void);
void Ticks(void);

#define LED_ON() HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin,GPIO_PIN_RESET)
#define LED_OFF() HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin,GPIO_PIN_SET)
#define LED_TOG() HAL_GPIO_TogglePin(LED_GPIO_Port,LED_Pin);



#endif