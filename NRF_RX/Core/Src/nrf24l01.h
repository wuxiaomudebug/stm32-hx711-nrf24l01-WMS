#ifndef __NRF24L01_HAL_H
#define __NRF24L01_HAL_H

#include "stm32f1xx_hal.h"
#include "spi.h"
#include <stdint.h>
#include <string.h>

// ========================= 核心参数配置（可直接替换）=========================
// 1. 引脚配置（根据你的硬件修改）
#define NRF24_CSN_PIN        GPIO_PIN_3
#define NRF24_CSN_PORT       GPIOA
#define NRF24_CE_PIN         GPIO_PIN_4
#define NRF24_CE_PORT        GPIOA

// 2. 通信参数配置（收发端必须一致）
#define NRF24_ADDR_WIDTH     5       // 地址宽度（3/4/5字节，默认5）
#define NRF24_PAYLOAD_SIZE  32       // 数据包长度（1-32字节，默认5）
#define NRF24_RF_CHANNEL    40      // 射频通道（0-127，默认40=2.440GHz）
#define NRF24_RETR_DELAY    0x04    // 重发延迟（0x00=250us，0x01=500us...0x0F=4000us，默认0x04=1000us）
#define NRF24_RETR_COUNT    0x0A    // 重发次数（0x00=禁用，0x01-0x0F=1-15次，默认0x0A=10次）
#define NRF24_RF_POWER      0x06    // 发射功率（0x02=-18dBm，0x04=-12dBm，0x06=-6dBm，0x07=0dBm，默认0x06）
#define NRF24_DATA_RATE     0x00    // 数据速率（0x00=1Mbps，0x01=2Mbps，默认0x00）
#define NRF24_CRC_MODE      0x01    // CRC模式（0x00=禁用，0x01=1字节，0x02=2字节，默认0x01）

// 3. 模式定义
#define NRF24_MODE_TX        0       // 发送模式
#define NRF24_MODE_RX        1       // 接收模式

// ========================= 寄存器/命令定义（无需修改）=========================
// 寄存器地址
#define NRF24_REG_CONFIG     0x00
#define NRF24_REG_EN_AA      0x01
#define NRF24_REG_EN_RXADDR  0x02
#define NRF24_REG_SETUP_AW   0x03
#define NRF24_REG_SETUP_RETR 0x04
#define NRF24_REG_RF_CH      0x05
#define NRF24_REG_RF_SETUP   0x06
#define NRF24_REG_STATUS     0x07
#define NRF24_REG_RX_ADDR_P0 0x0A
#define NRF24_REG_TX_ADDR    0x10
#define NRF24_REG_RX_PW_P0   0x11
#define NRF24_REG_FIFO_STATUS 0x17

// 命令
#define NRF24_CMD_W_REGISTER  0x20    // 写寄存器
#define NRF24_CMD_R_REGISTER  0x00    // 读寄存器
#define NRF24_CMD_W_TX_PAYLOAD 0xA0  // 写发送数据
#define NRF24_CMD_R_RX_PAYLOAD 0x61  // 读接收数据
#define NRF24_CMD_FLUSH_TX    0xE1    // 清空TX FIFO
#define NRF24_CMD_FLUSH_RX    0xE2    // 清空RX FIFO
#define NRF24_CMD_NOP        0xFF    // 空操作（读状态）

// 状态标志
#define NRF24_FLAG_RX_DR     0x40    // 接收完成标志
#define NRF24_FLAG_TX_DS     0x20    // 发送完成标志
#define NRF24_FLAG_MAX_RT   0x10    // 重发超限标志

// ========================= 函数声明（直接调用）=========================
/**
 * @brief  nRF24L01初始化
 * @param  mode: 工作模式（NRF24_MODE_TX / NRF24_MODE_RX）
 * @param  addr: 5字节通信地址（收发端必须一致）
 * @retval uint8_t: 0=初始化成功，1=初始化失败
 */
uint8_t NRF24_Init(uint8_t mode, const uint8_t *addr);

/**
 * @brief  发送数据包
 * @param  data: 发送数据缓冲区（长度=NRF24_PAYLOAD_SIZE）
 * @retval uint8_t: 0=发送成功，1=重发超限，2=发送失败
 */
uint8_t NRF24_TxPacket(const uint8_t *data);

/**
 * @brief  接收数据包
 * @param  data: 接收数据缓冲区（长度=NRF24_PAYLOAD_SIZE）
 * @retval uint8_t: 0=接收成功，1=无数据
 */
uint8_t NRF24_RxPacket(uint8_t *data);

/**
 * @brief  读取nRF24L01状态寄存器
 * @retval uint8_t: 状态值
 */
uint8_t NRF24_ReadStatus(void);

/**
 * @brief  清空FIFO
 * @param  is_tx: 1=清空TX FIFO，0=清空RX FIFO
 */
void NRF24_FlushFIFO(uint8_t is_tx);

#endif /* __NRF24L01_HAL_H */