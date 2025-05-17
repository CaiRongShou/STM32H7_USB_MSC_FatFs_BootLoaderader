#ifndef __BSP_FLASH_QSPI_H_
#define __BSP_FLASH_QSPI_H_


#include <stdint.h>
#include "main.h"
#define CMD_ENABLE_RESET	0x66
#define CMD_RESET			0x99


#define	CMD_GET_ID			0x9F
#define CMD_GET_UID			0x4B

#define CMD_WRITE_ENABLE	0x06

#define CMD_READ			0xEB
#define CMD_WRITE			0x32
#define CMD_ERASE_SECTOR	0x20
#define CMD_ERASE_BLOCK		0xD8
#define CMD_ERASE_CHIP		0xC7

#define CMD_GET_REG1		0x05
#define CMD_GET_REG2		0x35
#define CMD_GET_REG3		0x15

#define CMD_SET_REG1		0x01
#define CMD_SET_REG2		0x31
#define CMD_SET_REG3		0x11

#define SPI_FLASH_PageSize                  256


#define FALSH_QSPI_BASE				0x00000000
#define FALSH_QSPI_APP_ADDR			FALSH_QSPI_BASE
#define FALSH_QSPI_MSC_ADDR			FALSH_QSPI_APP_ADDR + 0x500000
#define FALSH_QSPI_USER_ADDR		FALSH_QSPI_MSC_ADDR + 0x200000

/***********************************************************************************
  									函数声明
***********************************************************************************/
extern uint8_t bsp_w25qxx_Init();
extern uint8_t bsp_w25qxx_Read(uint32_t addr , uint8_t*buff , uint32_t len);
extern uint8_t bsp_w25qxx_EraseSector(uint32_t addr);
extern uint8_t bsp_w25qxx_EraseBlock(uint32_t addr);
extern uint8_t bsp_w25qxx_EraseChip();
extern uint8_t bsp_w25qxx_Write(uint32_t addr , uint8_t*buff , uint32_t len);
extern uint8_t bsp_w25qxx_MemoryMapedEnter();
uint8_t QSPI_QuitMemoryMapped(void);
void bsp_w25qxx_test(void);

void SPI_FLASH_BufferWrite(u8* pBuffer, u32 WriteAddr, u16 NumByteToWrite);
void SPI_FLASH_BufferRead(u8* pBuffer, u32 ReadAddr, u16 NumByteToRead);
 
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
