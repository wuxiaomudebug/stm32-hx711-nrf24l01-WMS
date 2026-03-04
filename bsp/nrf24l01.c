#include "nrf24l01.h"

// 引脚操作宏（内部使用）
#define NRF24_CSN_HIGH()  HAL_GPIO_WritePin(NRF24_CSN_PORT, NRF24_CSN_PIN, GPIO_PIN_SET)
#define NRF24_CSN_LOW()   HAL_GPIO_WritePin(NRF24_CSN_PORT, NRF24_CSN_PIN, GPIO_PIN_RESET)
#define NRF24_CE_HIGH()   HAL_GPIO_WritePin(NRF24_CE_PORT, NRF24_CE_PIN, GPIO_PIN_SET)
#define NRF24_CE_LOW()    HAL_GPIO_WritePin(NRF24_CE_PORT, NRF24_CE_PIN, GPIO_PIN_RESET)

// 全局变量（内部使用）
static const uint8_t *nrf24_addr = NULL;  // 通信地址
static uint8_t nrf24_mode = 0;            // 工作模式

/**
 * @brief  SPI读写一个字节（内部使用）
 * @param  tx: 发送字节
 * @retval uint8_t: 接收字节
 */
static uint8_t NRF24_SPI_RW(uint8_t tx)
{
    uint8_t rx = 0;
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, 500);  // 依赖SPI1初始化（SCK=PA5, MOSI=PA7, MISO=PA6）
    return rx;
}

/**
 * @brief  写寄存器（内部使用）
 * @param  reg: 寄存器地址
 * @param  val: 要写入的值
 * @retval uint8_t: 状态值
 */
static uint8_t NRF24_WriteReg(uint8_t reg, uint8_t val)
{
    uint8_t status;
    NRF24_CSN_LOW();
    status = NRF24_SPI_RW(NRF24_CMD_W_REGISTER | (reg & 0x1F));
    NRF24_SPI_RW(val);
    NRF24_CSN_HIGH();
    return status;
}

/**
 * @brief  读寄存器（内部使用）
 * @param  reg: 寄存器地址
 * @retval uint8_t: 寄存器值
 */
static uint8_t NRF24_ReadReg(uint8_t reg)
{
    uint8_t val;
    NRF24_CSN_LOW();
    NRF24_SPI_RW(NRF24_CMD_R_REGISTER | (reg & 0x1F));
    val = NRF24_SPI_RW(NRF24_CMD_NOP);
    NRF24_CSN_HIGH();
    return val;
}

/**
 * @brief  写多字节寄存器（地址/数据）（内部使用）
 * @param  reg: 寄存器地址
 * @param  buf: 数据缓冲区
 * @param  len: 长度
 * @retval uint8_t: 状态值
 */
static uint8_t NRF24_WriteBuf(uint8_t reg, const uint8_t *buf, uint8_t len)
{
    uint8_t status, i;
    NRF24_CSN_LOW();
    status = NRF24_SPI_RW(NRF24_CMD_W_REGISTER | (reg & 0x1F));
    for(i = 0; i < len; i++)
    {
        NRF24_SPI_RW(buf[i]);
    }
    NRF24_CSN_HIGH();
    return status;
}

/**
 * @brief  断电重置（内部使用）
 */
static void NRF24_PowerOff(void)
{
    NRF24_CE_LOW();
    NRF24_WriteReg(NRF24_REG_CONFIG, 0x0D);
    NRF24_CE_HIGH();
    HAL_Delay(1);
    NRF24_CE_LOW();
}

/**
 * @brief  清空中断标志（内部使用）
 */
static void NRF24_ClearIRQ(void)
{
    NRF24_WriteReg(NRF24_REG_STATUS, 0x70);  // 清除RX_DR/TX_DS/MAX_RT
}

// ========================= 外部接口函数实现 =========================
uint8_t NRF24_Init(uint8_t mode, const uint8_t *addr)
{
    uint8_t reg_val;
    nrf24_mode = mode;
    nrf24_addr = addr;

    // 1. 引脚初始化（确保CubeMX中已配置为输出）
    NRF24_CE_LOW();
    NRF24_CSN_HIGH();

    // 2. 断电重置
    NRF24_PowerOff();

    // 3. 配置地址宽度
    switch(NRF24_ADDR_WIDTH)
    {
        case 3: reg_val = 0x01; break;
        case 4: reg_val = 0x02; break;
        case 5: reg_val = 0x03; break;
        default: return 1; // 地址宽度错误
    }
    NRF24_WriteReg(NRF24_REG_SETUP_AW, reg_val);

    // 4. 配置自动重发
    reg_val = (NRF24_RETR_DELAY << 4) | NRF24_RETR_COUNT;
    NRF24_WriteReg(NRF24_REG_SETUP_RETR, reg_val);

    // 5. 配置射频参数（功率+速率）
    reg_val = NRF24_RF_POWER | (NRF24_DATA_RATE << 3);
    NRF24_WriteReg(NRF24_REG_RF_SETUP, reg_val);

    // 6. 配置射频通道
    NRF24_WriteReg(NRF24_REG_RF_CH, NRF24_RF_CHANNEL & 0x7F);

    // 7. 配置地址（TX_ADDR和RX_ADDR_P0必须一致）
    NRF24_WriteBuf(NRF24_REG_TX_ADDR, addr, NRF24_ADDR_WIDTH);
    NRF24_WriteBuf(NRF24_REG_RX_ADDR_P0, addr, NRF24_ADDR_WIDTH);

    // 8. 配置接收数据长度
    NRF24_WriteReg(NRF24_REG_RX_PW_P0, NRF24_PAYLOAD_SIZE);

    // 9. 使能自动应答和接收通道
    NRF24_WriteReg(NRF24_REG_EN_AA, 0x01);     // 使能通道0自动应答
    NRF24_WriteReg(NRF24_REG_EN_RXADDR, 0x01); // 使能通道0接收

    // 10. 配置CONFIG寄存器（电源+CRC+模式）
    reg_val = 0x08; // PWR_UP=1（上电）
    reg_val |= (NRF24_CRC_MODE << 1); // CRC模式
    reg_val |= (mode == NRF24_MODE_RX) ? 0x01 : 0x00; // PRIM_RX=1（RX模式）/0（TX模式）
    NRF24_WriteReg(NRF24_REG_CONFIG, reg_val);

    // 11. 清空FIFO和中断标志
    NRF24_FlushFIFO(1);
    NRF24_FlushFIFO(0);
    NRF24_ClearIRQ();

    // 12. 激活模式
    if(mode == NRF24_MODE_RX)
    {
        NRF24_CE_HIGH();
        HAL_Delay(1); // 等待RX模式稳定
    }
    else
    {
        NRF24_CE_LOW();
    }

    // 13. 验证初始化（读CONFIG寄存器）
    if(NRF24_ReadReg(NRF24_REG_CONFIG) != reg_val)
    {
        return 1; // 初始化失败
    }
    return 0; // 初始化成功
}

uint8_t NRF24_TxPacket(const uint8_t *data)
{
    uint8_t status;
    if(nrf24_mode != NRF24_MODE_TX) return 2;

    NRF24_CE_LOW();
    NRF24_FlushFIFO(1); // 清空TX FIFO

    // 写发送数据
    NRF24_CSN_LOW();
    NRF24_SPI_RW(NRF24_CMD_W_TX_PAYLOAD);
    for(uint8_t i = 0; i < NRF24_PAYLOAD_SIZE; i++)
    {
        NRF24_SPI_RW(data[i]);
    }
    NRF24_CSN_HIGH();

    // 触发发送（CE高脉冲>10us）
    NRF24_CE_HIGH();
    HAL_Delay(1);
    NRF24_CE_LOW();

    // 等待发送结果
    HAL_Delay(20);
    status = NRF24_ReadStatus();

    if(status & NRF24_FLAG_TX_DS) // 发送成功
    {
        NRF24_ClearIRQ();
        return 0;
    }
    else if(status & NRF24_FLAG_MAX_RT) // 重发超限
    {
        NRF24_ClearIRQ();
        NRF24_FlushFIFO(1);
        return 1;
    }
    return 2; // 发送失败
}

uint8_t NRF24_RxPacket(uint8_t *data)
{
    uint8_t status, fifo_status;
    if(nrf24_mode != NRF24_MODE_RX) return 1;

    status = NRF24_ReadStatus();
    fifo_status = NRF24_ReadReg(NRF24_REG_FIFO_STATUS);

    // 检测是否有接收数据（RX_DR标志+FIFO非空）
    if((status & NRF24_FLAG_RX_DR) && ((fifo_status & 0x01) == 0))
    {
        // 读接收数据
        NRF24_CSN_LOW();
        NRF24_SPI_RW(NRF24_CMD_R_RX_PAYLOAD);
        for(uint8_t i = 0; i < NRF24_PAYLOAD_SIZE; i++)
        {
            data[i] = NRF24_SPI_RW(NRF24_CMD_NOP);
        }
        NRF24_CSN_HIGH();

        NRF24_ClearIRQ();
        NRF24_FlushFIFO(0);
        return 0; // 接收成功
    }
    return 1; // 无数据
}

uint8_t NRF24_ReadStatus(void)
{
    uint8_t status;
    NRF24_CSN_LOW();
    status = NRF24_SPI_RW(NRF24_CMD_NOP);
    NRF24_CSN_HIGH();
    return status;
}

void NRF24_FlushFIFO(uint8_t is_tx)
{
    NRF24_CSN_LOW();
    if(is_tx)
        NRF24_SPI_RW(NRF24_CMD_FLUSH_TX);
    else
        NRF24_SPI_RW(NRF24_CMD_FLUSH_RX);
    NRF24_CSN_HIGH();
    HAL_Delay(1);
}