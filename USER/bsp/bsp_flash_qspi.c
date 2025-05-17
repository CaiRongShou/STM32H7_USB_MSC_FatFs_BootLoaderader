/***********************************************************************************
  									头文件
***********************************************************************************/
#include "bsp_flash_qspi.h"
#include "string.h"
#include "quadspi.h"

/***********************************************************************************
  									宏定义
***********************************************************************************/
#define DEBUG_ENABLE
#define FLASH_DMA_IT	1
/***********************************************************************************
  									全局变量
***********************************************************************************/
extern QSPI_HandleTypeDef hqspi;

uint32_t 	ChipID;
uint64_t 	ChipUID;
uint8_t		StatusReg[3] ={0};
/***********************************************************************************
  									函数声明
***********************************************************************************/
static uint8_t 	bsp_w25qxx_AutoPollingNotBusy();
static uint8_t 	bsp_w25qxx_AutoPollingWriteEnable();
static uint8_t 	bsp_w25qxx_Reset();
static uint8_t 	bsp_w25qxx_GetID();
static uint8_t 	bsp_w25qxx_GetUID();
static uint8_t 	bsp_w25qxx_WriteEnable();
static void 	EndianSwap(uint8_t* pdata , uint8_t len);
static uint8_t 	bsp_w25qxx_WriteStatusReg(uint8_t regindex , uint8_t data);
static uint8_t 	bsp_w25qxx_ReadStatusReg(uint8_t regindex , uint8_t* data);
static int8_t   bsp_w25qxx_GetStatusRegs();
/***********************************************************************************
  									全局函数
***********************************************************************************/
/* 供本文件调用的全局变量 */
static __IO uint8_t CmdCplt, RxCplt, TxCplt, StatusMatch, TimeOut;

/**
***************************************************
* @brief    bsp_w25qxx_Init
* @note     初始化w25qxx芯片
* 			进行芯片复位，ID读取，UID读取
* @para		NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
uint8_t bsp_w25qxx_Init()
{
	//复位芯片
	if(bsp_w25qxx_Reset() != 0)
		return 1;

	//获取芯片ID信息
	if(bsp_w25qxx_GetID() != 0)
		return 1;

	//获取芯片UID信息
	if(bsp_w25qxx_GetUID() != 0)
		return 1;

	//使能QSPI模式  STATUS REG2 的QE写为1
	if(bsp_w25qxx_WriteStatusReg(2 , 0x02) != 0)
		return 1;

	//DRV 100%    WPS = 0
	if(bsp_w25qxx_WriteStatusReg(3 , 0) != 0)
		return 1;

	//获取状态寄存器
	if(bsp_w25qxx_GetStatusRegs() != 0)
		return 1;
	return 0;
}

/**
* @brief  读取FLASH数据
* @param 	pBuffer，存储读出数据的指针
* @param   ReadAddr，读取地址
* @param   NumByteToRead，读取数据长度
* @retval 无
*/
void SPI_FLASH_BufferRead(u8* pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
	bsp_w25qxx_Read(ReadAddr,pBuffer,NumByteToRead);
}

/**
* @brief  对FLASH按页写入数据，调用本函数写入数据前需要先擦除扇区
* @param	pBuffer，要写入数据的指针
* @param WriteAddr，写入地址
* @param  NumByteToWrite，写入数据长度，必须小于等于SPI_FLASH_PerWritePageSize
* @retval 无
*/
void SPI_FLASH_PageWrite(u8* pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
	bsp_w25qxx_Write(WriteAddr,pBuffer,NumByteToWrite);
}
 
  /**
   * @brief  对FLASH写入数据，调用本函数写入数据前需要先擦除扇区
   * @param	pBuffer，要写入数据的指针
   * @param  WriteAddr，写入地址
   * @param  NumByteToWrite，写入数据长度
   * @retval 无
   */
 void SPI_FLASH_BufferWrite(u8* pBuffer, u32 WriteAddr, u16 NumByteToWrite)
 {
   u8 NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;
     
     /*mod运算求余，若writeAddr是SPI_FLASH_PageSize整数倍，运算结果Addr值为0*/
   Addr = WriteAddr % SPI_FLASH_PageSize;
     
     /*差count个数据值，刚好可以对齐到页地址*/
   count = SPI_FLASH_PageSize - Addr;
     /*计算出要写多少整数页*/
   NumOfPage =  NumByteToWrite / SPI_FLASH_PageSize; 
     /*mod运算求余，计算出剩余不满一页的字节数*/
   NumOfSingle = NumByteToWrite % SPI_FLASH_PageSize;
     
     /* Addr=0,则WriteAddr 刚好按页对齐 aligned  */
   if (Addr == 0)
   {
         /* NumByteToWrite < SPI_FLASH_PageSize */
     if (NumOfPage == 0) 
     {
       SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumByteToWrite);
     }
     else /* NumByteToWrite > SPI_FLASH_PageSize */
     { 
             /*先把整数页都写了*/
       while (NumOfPage--)
       {
         SPI_FLASH_PageWrite(pBuffer, WriteAddr, SPI_FLASH_PageSize);
         WriteAddr +=  SPI_FLASH_PageSize;
         pBuffer += SPI_FLASH_PageSize;
       }
             /*若有多余的不满一页的数据，把它写完*/
	   if(NumOfSingle)
			SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumOfSingle);
     }
   }
     /* 若地址与 SPI_FLASH_PageSize 不对齐  */
   else 
   {
         /* NumByteToWrite < SPI_FLASH_PageSize */
     if (NumOfPage == 0)
     {
             /*当前页剩余的count个位置比NumOfSingle小，一页写不完*/
       if (NumOfSingle > count) 
       {
         temp = NumOfSingle - count;
                 /*先写满当前页*/
         SPI_FLASH_PageWrite(pBuffer, WriteAddr, count);
                 
         WriteAddr +=  count;
         pBuffer += count;
                 /*再写剩余的数据*/
         SPI_FLASH_PageWrite(pBuffer, WriteAddr, temp);
       }
       else /*当前页剩余的count个位置能写完NumOfSingle个数据*/
       {
         SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumByteToWrite);
       }
     }
     else /* NumByteToWrite > SPI_FLASH_PageSize */
     {
             /*地址不对齐多出的count分开处理，不加入这个运算*/
       NumByteToWrite -= count;
       NumOfPage =  NumByteToWrite / SPI_FLASH_PageSize;
       NumOfSingle = NumByteToWrite % SPI_FLASH_PageSize;
             
             /* 先写完count个数据，为的是让下一次要写的地址对齐 */
       SPI_FLASH_PageWrite(pBuffer, WriteAddr, count);
             
             /* 接下来就重复地址对齐的情况 */
       WriteAddr +=  count;
       pBuffer += count;
             /*把整数页都写了*/
       while (NumOfPage--)
       {
         SPI_FLASH_PageWrite(pBuffer, WriteAddr, SPI_FLASH_PageSize);
         WriteAddr +=  SPI_FLASH_PageSize;
         pBuffer += SPI_FLASH_PageSize;
       }
             /*若有多余的不满一页的数据，把它写完*/
       if (NumOfSingle != 0)
       {
         SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumOfSingle);
       }
     }
   }
 }
 
/**
***************************************************
* @brief    bsp_w25qxx_Read
* @note     读取芯片数据
* @para
* 			addr		读取地址
* 			buff		存放缓冲
* 			len			读取长度
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
uint8_t bsp_w25qxx_Read(uint32_t addr , uint8_t*buff , uint32_t len)
{
    QSPI_CommandTypeDef command = {0};
	
	/* 用于等待接收完成标志 */
	RxCplt = 0;
	
    command.InstructionMode 		= QSPI_INSTRUCTION_1_LINE;          /*1线方式发送指令 */
    command.AddressSize 			= QSPI_ADDRESS_24_BITS;             /*24位地址 */
    command.AlternateByteMode 		= QSPI_ALTERNATE_BYTES_NONE;     	/*无交替字节 */
    command.DdrMode 				= QSPI_DDR_MODE_DISABLE;            /*W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 		= QSPI_DDR_HHC_ANALOG_DELAY;      	/*DDR模式，数据输出延迟 */
    command.SIOOMode 				= QSPI_SIOO_INST_EVERY_CMD;    		/*每次发送命令 */

    command.Instruction 			= CMD_READ;   						/*读取数据*/
    command.DummyCycles 			= 6;                                /*6个空周期*/
    command.AddressMode 			= QSPI_ADDRESS_4_LINES;             /*4地址信息*/
    command.DataMode 				= QSPI_DATA_4_LINES;                /*4线数据*/
    command.NbData 					= len;                             	/*读取数据大小 */
    command.Address 				= addr;                            	/*读取地址 */

    if (HAL_QSPI_Command(&hqspi, &command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		return 1;
	}

#if FLASH_DMA_IT
	/* MDMA方式读取 */
	if (HAL_QSPI_Receive_DMA(&hqspi, buff) != HAL_OK)
	{
		return 1;
	}	
	
	/* 等接受完毕 */
	while(RxCplt == 0);
	RxCplt = 0;
#else
	
		
	if (HAL_QSPI_Receive(&hqspi, buff,1000) != HAL_OK)
	{
		return 1;
	}
#endif
	return 0;
}
/**
***************************************************
* @brief    bsp_w25qxx_EraseSector
* @note     擦除4k数据
* @para		NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
uint8_t bsp_w25qxx_EraseSector(uint32_t addr)
{
    QSPI_CommandTypeDef command = {0};
	
	/* 用于命令发送完成标志 */	
	CmdCplt = 0;
	
    //WEL使能
    if(bsp_w25qxx_WriteEnable() != 0)
    	return 1;

    command.InstructionMode 		= QSPI_INSTRUCTION_1_LINE;          /*1线方式发送指令 */
    command.AddressSize 			= QSPI_ADDRESS_24_BITS;             /*24位地址 */
    command.AlternateByteMode 		= QSPI_ALTERNATE_BYTES_NONE;     	/*无交替字节 */
    command.DdrMode 				= QSPI_DDR_MODE_DISABLE;            /*W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 		= QSPI_DDR_HHC_ANALOG_DELAY;      	/*DDR模式，数据输出延迟 */
    command.SIOOMode 				= QSPI_SIOO_INST_EVERY_CMD;    		/*每次发送命令 */

    command.Instruction 			= CMD_ERASE_SECTOR;   				/*擦除sector 4k*/
    command.DummyCycles 			= 0;                                /*不需要空周期*/
    command.AddressMode 			= QSPI_ADDRESS_1_LINE;              /*1地址信息*/
    command.DataMode 				= QSPI_DATA_NONE;                	/*0线数据*/
    command.NbData 					= 0;                             	/*数据*/
    command.Address 				= addr;                            	/*地址 */

#if FLASH_DMA_IT
    //发送命令
	if (HAL_QSPI_Command_IT(&hqspi, &command) != HAL_OK)
		return 1;
	
	/* 等待命令发送完毕 */
	while(CmdCplt == 0);
	CmdCplt = 0;
	
	/* 等待编程结束 */
	StatusMatch = 0;
	bsp_w25qxx_AutoPollingNotBusy();	
	while(StatusMatch == 0);
	StatusMatch = 0;
#else
	if (HAL_QSPI_Command(&hqspi, &command,1000) != HAL_OK)
		return 1;	
#endif
	
	
	return 0;
}

/**
***************************************************
* @brief    bsp_w25qxx_EraseBlock
* @note     擦除64k数据
* @para		NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
uint8_t bsp_w25qxx_EraseBlock(uint32_t addr)
{
    QSPI_CommandTypeDef command = {0};
	
	/* 用于命令发送完成标志 */	
	CmdCplt = 0;
	
    //WEL使能
    if(bsp_w25qxx_WriteEnable() != 0)
    	return 1;


    command.InstructionMode 		= QSPI_INSTRUCTION_1_LINE;          /*1线方式发送指令 */
    command.AddressSize 			= QSPI_ADDRESS_24_BITS;             /*24位地址 */
    command.AlternateByteMode 		= QSPI_ALTERNATE_BYTES_NONE;     	/*无交替字节 */
    command.DdrMode 				= QSPI_DDR_MODE_DISABLE;            /*W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 		= QSPI_DDR_HHC_ANALOG_DELAY;      	/*DDR模式，数据输出延迟 */
    command.SIOOMode 				= QSPI_SIOO_INST_EVERY_CMD;    		/*每次发送命令 */


    command.Instruction 			= CMD_ERASE_BLOCK;   				/*擦除block 64k*/
    command.DummyCycles 			= 0;                                /*不需要空周期*/
    command.AddressMode 			= QSPI_ADDRESS_1_LINE;              /*1地址信息*/
    command.DataMode 				= QSPI_DATA_NONE;                	/*0线数据*/
    command.NbData 					= 0;                             	/*数据*/
    command.Address 				= addr;                            	/*地址 */

#if FLASH_DMA_IT
    //发送命令
	if (HAL_QSPI_Command_IT(&hqspi, &command) != HAL_OK)
		return 1;
	
	/* 等待命令发送完毕 */
	while(CmdCplt == 0);
	CmdCplt = 0;
	
	/* 等待编程结束 */
	StatusMatch = 0;
	bsp_w25qxx_AutoPollingNotBusy();	
	while(StatusMatch == 0);
	StatusMatch = 0;
#else
	//发送命令
	if (HAL_QSPI_Command(&hqspi, &command,1000) != HAL_OK)
		return 1;
	bsp_w25qxx_AutoPollingNotBusy();	

#endif
	return 0;
}
/**
***************************************************
* @brief    bsp_w25qxx_EraseChip
* @note     正片擦除
* @para		NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
uint8_t bsp_w25qxx_EraseChip()
{
    QSPI_CommandTypeDef command = {0};
	
	/* 用于命令发送完成标志 */	
	CmdCplt = 0;
	
    //WEL使能
    if(bsp_w25qxx_WriteEnable() != 0)
    	return 1;

    command.InstructionMode 		= QSPI_INSTRUCTION_1_LINE;          /*1线方式发送指令 */
    command.AddressSize 			= QSPI_ADDRESS_24_BITS;             /*24位地址 */
    command.AlternateByteMode 		= QSPI_ALTERNATE_BYTES_NONE;     	/*无交替字节 */
    command.DdrMode 				= QSPI_DDR_MODE_DISABLE;            /*W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 		= QSPI_DDR_HHC_ANALOG_DELAY;      	/*DDR模式，数据输出延迟 */
    command.SIOOMode 				= QSPI_SIOO_INST_EVERY_CMD;    		/*每次发送命令 */


    command.Instruction 			= CMD_ERASE_CHIP;   				/*擦除chip*/
    command.DummyCycles 			= 0;                                /*不需要空周期*/
    command.AddressMode 			= QSPI_ADDRESS_NONE;                /*无地址信息*/
    command.DataMode 				= QSPI_DATA_NONE;                	/*0线数据*/
    command.NbData 					= 0;                             	/*数据*/

#if FLASH_DMA_IT
    //发送命令
	if (HAL_QSPI_Command_IT(&hqspi, &command) != HAL_OK)
		return 1;
	
	/* 等待命令发送完毕 */
	while(CmdCplt == 0);
	CmdCplt = 0;
	
	/* 等待编程结束 */
	StatusMatch = 0;
	bsp_w25qxx_AutoPollingNotBusy();	
	while(StatusMatch == 0);
	StatusMatch = 0;
	
	return 0;
#else 
	//发送命令
	if (HAL_QSPI_Command(&hqspi, &command,1000) != HAL_OK)
		return 1;
	return bsp_w25qxx_AutoPollingNotBusy();
#endif

	
}
/**
***************************************************
* @brief    bsp_w25qxx_Write
* @note     写入芯片数据,最大写入1页256个数据，大于256，从头写
* @para
* 			addr		写入地址
* 			buff		写入数据缓冲区
* 			len			写入长度
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
uint8_t bsp_w25qxx_Write(uint32_t addr , uint8_t*buff , uint32_t len)
{
    QSPI_CommandTypeDef command = {0};

	TxCplt = 0;
    //WEL使能
    if(bsp_w25qxx_WriteEnable() != 0)
    	return 1;

    command.InstructionMode 		= QSPI_INSTRUCTION_1_LINE;          /*1线方式发送指令 */
    command.AddressSize 			= QSPI_ADDRESS_24_BITS;             /*24位地址 */
    command.AlternateByteMode 		= QSPI_ALTERNATE_BYTES_NONE;     	/*无交替字节 */
    command.DdrMode 				= QSPI_DDR_MODE_DISABLE;            /*W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 		= QSPI_DDR_HHC_ANALOG_DELAY;      	/*DDR模式，数据输出延迟 */
    command.SIOOMode 				= QSPI_SIOO_INST_ONLY_FIRST_CMD;    /*只发送一次*/


    command.Instruction 			= CMD_WRITE;   						/*写入数据*/
    command.DummyCycles 			= 0;                                /*不需要空周期*/
    command.AddressMode 			= QSPI_ADDRESS_1_LINE;              /*1地址信息*/
    command.DataMode 				= QSPI_DATA_4_LINES;                /*4线数据*/
    command.NbData 					= len;                             	/*数据*/
    command.Address 				= addr;                            	/*地址 */

#if FLASH_DMA_IT

    //发送写命令
	if (HAL_QSPI_Command_IT(&hqspi, &command) != HAL_OK)
		return 1;
	//写数据
	if (HAL_QSPI_Transmit_DMA(&hqspi, buff) != HAL_OK)
		return 1;
	
	/* 等待数据发送完毕 */
	while(TxCplt == 0);
	TxCplt = 0;
	
	/* 等待Flash页编程完毕 */
	StatusMatch = 0;
	bsp_w25qxx_AutoPollingNotBusy();	
	while(StatusMatch == 0);
	StatusMatch = 0;
#else	
    //发送写命令
	if (HAL_QSPI_Command(&hqspi, &command,1000) != HAL_OK)
		return 1;
	//写数据
	if (HAL_QSPI_Transmit(&hqspi, buff,1000) != HAL_OK)
		return 1;
	
	bsp_w25qxx_AutoPollingNotBusy();
#endif
	return 0;
}
/**
***************************************************
* @brief    bsp_w25qxx_MemoryMapedEnter
* @note     QSPI进入内存映射模式，地址为0x90000000
* @para		NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
uint8_t bsp_w25qxx_MemoryMapedEnter()
{
	QSPI_CommandTypeDef 		command = {0};
	QSPI_MemoryMappedTypeDef 	mappedcfg = {0};

	command.InstructionMode 	= QSPI_INSTRUCTION_1_LINE;      	/* 1线方式发送指令 */
	command.AddressSize 		= QSPI_ADDRESS_24_BITS;             /* 32位地址 */
	command.AlternateByteMode 	= QSPI_ALTERNATE_BYTES_NONE;  		/* 无交替字节 */
	command.DdrMode 			= QSPI_DDR_MODE_DISABLE;            /* W25Q256JV不支持DDR */
	command.DdrHoldHalfCycle 	= QSPI_DDR_HHC_ANALOG_DELAY;   		/* DDR模式，数据输出延迟 */
	command.SIOOMode 			= QSPI_SIOO_INST_EVERY_CMD;         /* 每次传输都发指令 */

	command.Instruction 		= CMD_READ; 						/* 快速读取命令 */
	command.AddressMode 		= QSPI_ADDRESS_4_LINES;             /* 4个地址线 */
	command.DataMode 			= QSPI_DATA_4_LINES;                /* 4个数据线 */
	command.DummyCycles 		= 6;                                /* 空周期 */

	/* 关闭溢出计数 */
	mappedcfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
	mappedcfg.TimeOutPeriod = 0;
	if (HAL_QSPI_MemoryMapped(&hqspi, &command, &mappedcfg) != HAL_OK)
		return 1;
	return 0;
}

/*
*********************************************************************************************************
*    函 数 名: QSPI_QuitMemoryMapped
*    功能说明: 退出QSPI内存映射，地址 0x90000000
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint8_t QSPI_QuitMemoryMapped(void)
{
    HAL_QSPI_Abort(&hqspi);
 
    return 0;
}



/*
*********************************************************************************************************
*	函 数 名: HAL_QSPI_CmdCpltCallback
*	功能说明: QSPI中断的回调函数，Flash擦除函数QSPI_EraseSector要使用
*	形    参: hqspi  QSPI_HandleTypeDef句柄
*	返 回 值: 无
*********************************************************************************************************
*/
void HAL_QSPI_CmdCpltCallback(QSPI_HandleTypeDef *hqspi)
{
	CmdCplt++;
}

/*
*********************************************************************************************************
*	函 数 名: HAL_QSPI_RxCpltCallback
*	功能说明: QSPI中断的回调函数，Flash读函数QSPI_ReadBuffer要使用
*	形    参: hqspi  QSPI_HandleTypeDef句柄
*	返 回 值: 无
*********************************************************************************************************
*/
void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
	RxCplt++;
}

/*
*********************************************************************************************************
*	函 数 名: HAL_QSPI_TxCpltCallback
*	功能说明: QSPI中断的回调函数，Flash写函数QSPI_ReadBuffer要使用
*	形    参: hqspi  QSPI_HandleTypeDef句柄
*	返 回 值: 无
*********************************************************************************************************
*/
 void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
	TxCplt++; 
}

/*
*********************************************************************************************************
*	函 数 名: HAL_QSPI_StatusMatchCallback
*	功能说明: QSPI中断的回调函数，Flash状态查询函数QSPI_AutoPollingMemReady使用
*	形    参: hqspi  QSPI_HandleTypeDef句柄
*	返 回 值: 无
*********************************************************************************************************
*/
void HAL_QSPI_StatusMatchCallback(QSPI_HandleTypeDef *hqspi)
{
	StatusMatch++;
}



/***********************************************************************************
  									局部函数
***********************************************************************************/
/**
***************************************************
* @brief    EndianSwap
* @note     数据大小端转换
* @para
* 			padat	指向待转换的数据
* 			len		数据为几字节
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
static void EndianSwap(uint8_t* pdata , uint8_t len)
{
	uint8_t temp = 0;

	for(uint8_t i=0;i<len/2;i++)
	{
		temp = pdata[i];
		pdata[i] = pdata[len-i-1];
		pdata[len-i-1] = temp;
	}
}

/**
***************************************************
* @brief    bsp_w25qxx_AutoPollingNotBusy
* @note     等待BUSY信号
* @para     NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
static uint8_t bsp_w25qxx_AutoPollingNotBusy()
{
    QSPI_CommandTypeDef 		command = {0};
    QSPI_AutoPollingTypeDef 	config = {0};

    command.InstructionMode 	= QSPI_INSTRUCTION_1_LINE;           /* 1线方式发送指令 */
    command.AddressSize 		= QSPI_ADDRESS_24_BITS;              /* 32位地址 */
    command.AlternateByteMode 	= QSPI_ALTERNATE_BYTES_NONE;         /* 无交替字节 */
    command.DdrMode 			= QSPI_DDR_MODE_DISABLE;             /* W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 	= QSPI_DDR_HHC_ANALOG_DELAY;         /* DDR模式，数据输出延迟 */
    command.SIOOMode 			= QSPI_SIOO_INST_EVERY_CMD;          /* 每次传输都发指令 */

    command.Instruction 		= CMD_GET_REG1;                      /* 读取状态命令 */
    command.AddressMode 		= QSPI_ADDRESS_NONE;                 /* 无需地址 */
    command.DataMode 			= QSPI_DATA_1_LINE;                  /* 1线数据 */
    command.DummyCycles 		= 0;                                 /* 无需空周期 */

    /* 屏蔽位设置的bit0，匹配位等待bit0为0，即不断查询状态寄存器bit0，等待其为0 */
    config.Match 			= 0x00;
    config.Mask 			= 0x01;
    config.Interval 		= 0x10;
    config.StatusBytesSize 	= 1;
    config.MatchMode 		= QSPI_MATCH_MODE_AND;
    config.AutomaticStop 	= QSPI_AUTOMATIC_STOP_ENABLE;
#if FLASH_DMA_IT
    if (HAL_QSPI_AutoPolling_IT(&hqspi, &command, &config) != HAL_OK)
		return 1;
#else	
	if (HAL_QSPI_AutoPolling(&hqspi, &command, &config,HAL_MAX_DELAY) != HAL_OK)
		return 1;
#endif
    return 0;
}
/**
***************************************************
* @brief    bsp_w25qxx_AutoPollingWriteEnable
* @note     等待WEL信号
* @para     NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
static uint8_t bsp_w25qxx_AutoPollingWriteEnable()
{
    QSPI_CommandTypeDef 		command = {0};
    QSPI_AutoPollingTypeDef 	config = {0};

    command.InstructionMode 	= QSPI_INSTRUCTION_1_LINE;           /* 1线方式发送指令 */
    command.AddressSize 		= QSPI_ADDRESS_24_BITS;              /* 32位地址 */
    command.AlternateByteMode 	= QSPI_ALTERNATE_BYTES_NONE;         /* 无交替字节 */
    command.DdrMode 			= QSPI_DDR_MODE_DISABLE;             /* W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 	= QSPI_DDR_HHC_ANALOG_DELAY;         /* DDR模式，数据输出延迟 */
    command.SIOOMode 			= QSPI_SIOO_INST_EVERY_CMD;          /* 每次传输都发指令 */

    command.Instruction 		= CMD_GET_REG1;                      /* 读取状态命令 */
    command.AddressMode 		= QSPI_ADDRESS_NONE;                 /* 无需地址 */
    command.DataMode 			= QSPI_DATA_1_LINE;                  /* 1线数据 */
    command.DummyCycles 		= 0;                                 /* 无需空周期 */

    /* 屏蔽位设置的bit0，匹配位等待bit0为0，即不断查询状态寄存器bit0，等待其为0 */
    config.Mask 			= 0xFD;
    config.Match 			= 0x01;
    config.MatchMode 		= QSPI_MATCH_MODE_OR;
    config.StatusBytesSize 	= 1;
    config.Interval 		= 0x10;
    config.AutomaticStop 	= QSPI_AUTOMATIC_STOP_ENABLE;

    if (HAL_QSPI_AutoPolling(&hqspi, &command, &config, 1000) != HAL_OK)
		return 1;
    return 0;
}

/**
***************************************************
* @brief    bsp_w25qxx_Reset
* @note     复位芯片，66和99命令必须连续发送，发送完毕后至少需要等待30us
* @para     NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
static uint8_t bsp_w25qxx_Reset()
{
    QSPI_CommandTypeDef command = {0};

    command.InstructionMode 		= QSPI_INSTRUCTION_1_LINE;          /*1线方式发送指令 */
    command.AddressSize 			= QSPI_ADDRESS_24_BITS;             /*32位地址 */
    command.AlternateByteMode 		= QSPI_ALTERNATE_BYTES_NONE;     	/*无交替字节 */
    command.DdrMode 				= QSPI_DDR_MODE_DISABLE;            /*W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 		= QSPI_DDR_HHC_ANALOG_DELAY;      	/*DDR模式，数据输出延迟 */
    command.SIOOMode 				= QSPI_SIOO_INST_EVERY_CMD;    		/*每次发送命令 */

    command.Instruction 			= CMD_ENABLE_RESET;   				/*使能重启*/
    command.DummyCycles 			= 0;                                /*不需要空周期*/
    command.AddressMode 			= QSPI_ADDRESS_NONE;              	/*无地址信息*/
    command.DataMode 				= QSPI_DATA_NONE;                   /*无数据信息*/
    command.NbData 					= 0;                             	/*写数据大小 */
    command.Address 				= 0;                            	/*写入地址 */

    /*复位使能*/
    if (HAL_QSPI_Command(&hqspi, &command, 1000) != HAL_OK)
		return 1;
 
	/*复位*/
    command.Instruction 			= CMD_RESET;
    if (HAL_QSPI_Command(&hqspi, &command, 1000) != HAL_OK)
		return 1;
    return 0;
}

/**
***************************************************
* @brief    bsp_w25qxx_GetID
* @note     获取芯片的ID信息，识别芯片类型
* @para     NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
static uint8_t bsp_w25qxx_GetID()
{
    QSPI_CommandTypeDef command = {0};


    command.InstructionMode 		= QSPI_INSTRUCTION_1_LINE;          /*1线方式发送指令 */
    command.AddressSize 			= QSPI_ADDRESS_24_BITS;             /*32位地址 */
    command.AlternateByteMode 		= QSPI_ALTERNATE_BYTES_NONE;     	/*无交替字节 */
    command.DdrMode 				= QSPI_DDR_MODE_DISABLE;            /*W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 		= QSPI_DDR_HHC_ANALOG_DELAY;      	/*DDR模式，数据输出延迟 */
    command.SIOOMode 				= QSPI_SIOO_INST_EVERY_CMD;    		/*每次发送命令 */


    command.Instruction 			= CMD_GET_ID;   					/*获取芯片ID*/
    command.DummyCycles 			= 0;                                /*不需要空周期*/
    command.AddressMode 			= QSPI_ADDRESS_NONE;              	/*无地址信息*/
    command.DataMode 				= QSPI_DATA_1_LINE;                   /*无数据信息*/
    command.NbData 					= 3;                             	/*写数据大小 */
    command.Address 				= 0;                            	/*写入地址 */

    //发送获取ID命令
	if (HAL_QSPI_Command(&hqspi, &command, 1000) != HAL_OK)
		return 1;
	//获取ID
	if (HAL_QSPI_Receive(&hqspi, (uint8_t *)&ChipID, 1000) != HAL_OK)
		return 1;
	//进行大小端转换
	EndianSwap((uint8_t *)&ChipID , 3);
	return 0;
}
/**
***************************************************
* @brief    bsp_w25qxx_GetUID
* @note     识别芯片的UID编码
* @para     NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
static uint8_t bsp_w25qxx_GetUID()
{
    QSPI_CommandTypeDef command = {0};

    command.InstructionMode 		= QSPI_INSTRUCTION_1_LINE;          /*1线方式发送指令 */
    command.AddressSize 			= QSPI_ADDRESS_24_BITS;             /*32位地址 */
    command.AlternateByteMode 		= QSPI_ALTERNATE_BYTES_NONE;     	/*无交替字节 */
    command.DdrMode 				= QSPI_DDR_MODE_DISABLE;            /*W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 		= QSPI_DDR_HHC_ANALOG_DELAY;      	/*DDR模式，数据输出延迟 */
    command.SIOOMode 				= QSPI_SIOO_INST_EVERY_CMD;    		/*每次发送命令 */

    command.Instruction 			= CMD_GET_UID;   					/*获取芯片UID*/
    command.DummyCycles 			= 4;                                /*不需要空周期*/
    command.AddressMode 			= QSPI_ADDRESS_NONE;              	/*无地址信息*/
    command.DataMode 				= QSPI_DATA_1_LINE;                 /*无数据信息*/
    command.NbData 					= 8;                             	/*写数据大小 */
    command.Address 				= 0;                            	/*写入地址 */

    //发送命令
	if (HAL_QSPI_Command(&hqspi, &command, 1000) != HAL_OK)
		return 1;
    //接收UID信息
	if (HAL_QSPI_Receive(&hqspi, (uint8_t *)&ChipUID, 1000) != HAL_OK)
		return 1;
	//大小端抓换
	EndianSwap((uint8_t *)&ChipUID , 8);
	return 0;
}
/**
***************************************************
* @brief    bsp_w25qxx_WriteEnable
* @note     使能W25QXX的写功能，使能状态寄存器的WEL位
* @para     NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
static uint8_t 	bsp_w25qxx_WriteEnable()
{
    QSPI_CommandTypeDef command = {0};

    command.InstructionMode 		= QSPI_INSTRUCTION_1_LINE;          /*1线方式发送指令 */
    command.AddressSize 			= QSPI_ADDRESS_24_BITS;             /*32位地址 */
    command.AlternateByteMode 		= QSPI_ALTERNATE_BYTES_NONE;     	/*无交替字节 */
    command.DdrMode 				= QSPI_DDR_MODE_DISABLE;            /*W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 		= QSPI_DDR_HHC_ANALOG_DELAY;      	/*DDR模式，数据输出延迟 */
    command.SIOOMode 				= QSPI_SIOO_INST_EVERY_CMD;    		/*每次发送命令 */

    command.Instruction 			= CMD_WRITE_ENABLE;   				/*写使能*/
    command.DummyCycles 			= 0;                                /*不需要空周期*/
    command.AddressMode 			= QSPI_ADDRESS_NONE;              	/*无地址信息*/
    command.DataMode 				= QSPI_DATA_NONE;                   /*无数据信息*/
    command.NbData 					= 0;                             	/*写数据大小 */
    command.Address 				= 0;                            	/*写入地址 */

	//发送命令
	if (HAL_QSPI_Command(&hqspi, &command, 1000) != HAL_OK)
		return 1;
	//等待WEL位置位
	return bsp_w25qxx_AutoPollingWriteEnable();
}

/**
***************************************************
* @brief    bsp_w25qxx_WriteStatusReg
* @note     使能W25QXX的QSPI模式
* @para
			regindex	写第几个
			data		写入内容
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
static uint8_t 	bsp_w25qxx_WriteStatusReg(uint8_t regindex , uint8_t data)
{
    //使能WEL
	if(bsp_w25qxx_WriteEnable() != 0)
		return 1;

    QSPI_CommandTypeDef command = {0};

    command.InstructionMode 		= QSPI_INSTRUCTION_1_LINE;          /*1线方式发送指令 */
    command.AddressSize 			= QSPI_ADDRESS_24_BITS;             /*32位地址 */
    command.AlternateByteMode 		= QSPI_ALTERNATE_BYTES_NONE;     	/*无交替字节 */
    command.DdrMode 				= QSPI_DDR_MODE_DISABLE;            /*W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 		= QSPI_DDR_HHC_ANALOG_DELAY;      	/*DDR模式，数据输出延迟 */
    command.SIOOMode 				= QSPI_SIOO_INST_EVERY_CMD;    		/*每次发送命令 */

    /* 写序列配置 */
    if(regindex == 1)
        command.Instruction 		= CMD_SET_REG1;   					/*写status reg1*/
    else if(regindex == 2)
        command.Instruction 		= CMD_SET_REG2;   					/*写status reg2*/
    else if(regindex == 3)
        command.Instruction 		= CMD_SET_REG3;   					/*写status reg3*/
    command.DummyCycles 			= 0;                                /*不需要空周期*/
    command.AddressMode 			= QSPI_ADDRESS_NONE;              	/*无地址信息*/
    command.DataMode 				= QSPI_DATA_1_LINE;                 /*无数据信息*/
    command.NbData 					= 1;                             	/*写数据大小 */
    command.Address 				= 0;                            	/*写入地址 */

    //发送配置命令
	if (HAL_QSPI_Command(&hqspi, &command, 1000) != HAL_OK)
		return 1;
	//发送配置信息
	if(HAL_QSPI_Transmit(&hqspi, &data, 1000) != HAL_OK)
		return 1;
	//等待notbusy
	return bsp_w25qxx_AutoPollingNotBusy();
}
/**
***************************************************
* @brief    bsp_w25qxx_ReadStatusReg
* @note     读取status寄存器
* @para
* 			regindex	读第几个
* 			data		存放读取到的数据
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
static uint8_t 	bsp_w25qxx_ReadStatusReg(uint8_t regindex , uint8_t* data)
{
    QSPI_CommandTypeDef command = {0};

    command.InstructionMode 		= QSPI_INSTRUCTION_1_LINE;          /*1线方式发送指令 */
    command.AddressSize 			= QSPI_ADDRESS_24_BITS;             /*32位地址 */
    command.AlternateByteMode 		= QSPI_ALTERNATE_BYTES_NONE;     	/*无交替字节 */
    command.DdrMode 				= QSPI_DDR_MODE_DISABLE;            /*W25Q256JV不支持DDR */
    command.DdrHoldHalfCycle 		= QSPI_DDR_HHC_ANALOG_DELAY;      	/*DDR模式，数据输出延迟 */
    command.SIOOMode 				= QSPI_SIOO_INST_EVERY_CMD;    		/*每次发送命令 */

    if(regindex == 1)
        command.Instruction 		= CMD_GET_REG1;   					/*写status reg1*/
    else if(regindex == 2)
        command.Instruction 		= CMD_GET_REG2;   					/*写status reg2*/
    else if(regindex == 3)
        command.Instruction 		= CMD_GET_REG3;   					/*写status reg3*/
    command.DummyCycles 			= 0;                                /*不需要空周期*/
    command.AddressMode 			= QSPI_ADDRESS_NONE;              	/*无地址信息*/
    command.DataMode 				= QSPI_DATA_1_LINE;                 /*无数据信息*/
    command.NbData 					= 1;                             	/*数据大小 */
    command.Address 				= 0;                            	/*入地址 */

    //发送读取命令
	if (HAL_QSPI_Command(&hqspi, &command, 1000) != HAL_OK)
		return 1;
	//接收数据
	if(HAL_QSPI_Receive(&hqspi, data, 1000) != HAL_OK)
		return 1;
	return 0;
}
/**
***************************************************
* @brief    bsp_w25qxx_GetStatusRegs
* @note     获取三个status register 数据
* @para		NONE
* @retval
			0：成功
			1：失败
* @data     2022.04.28
* @auth     WXL
***************************************************
**/
static int8_t bsp_w25qxx_GetStatusRegs()
{
    if(bsp_w25qxx_ReadStatusReg(1 , &StatusReg[0]) != 0)
    	return 1;
    if(bsp_w25qxx_ReadStatusReg(2 , &StatusReg[1]) != 0)
    	return 1;
    if(bsp_w25qxx_ReadStatusReg(3 , &StatusReg[2]) != 0)
    	return 1;
    return 0;
}
#ifdef DEBUG_ENABLE
void bsp_w25qxx_test(void)
{

	#define SIZE 		4096
	#define	ADDRESS		FALSH_QSPI_MSC_ADDR
	volatile uint8_t buff[SIZE];
	volatile uint16_t index = 0;


	printf("Init w25qxx success\r\n");
	printf("Chip ID is %06x\r\n",ChipID);
	printf("Chip UID is %llx\r\n",ChipUID);
	printf("Status   REG1:%02x  REG2:%02x  REG3:%02x  \r\n",StatusReg[0],StatusReg[1],StatusReg[2]);

	//测试sector的擦除功能。
//	printf("\r\nErase sector test\r\n");
//	bsp_w25qxx_EraseSector(ADDRESS);
//	HAL_Delay(100);
//	printf("Read data from chip\r\n");
//	bsp_w25qxx_Read(ADDRESS , buff , SIZE);
//	HAL_Delay(100);
//	for(index = 0;index <SIZE;index++)
//	{
//		if((index %15 == 0) && (index!=0))
//		{
//			printf("\r\n");
//		}
//		printf("  %02x  ",buff[index]);
//		buff[index] = index % SIZE;
//	}
//	memset(buff , 0xaa , SIZE);
//	printf("\r\nWrite data to chip\r\n");
//	SPI_FLASH_BufferWrite(buff ,ADDRESS ,  SIZE);
//	HAL_Delay(100);
//	memset(buff , 0 , SIZE);
//	printf("Get data from chip\r\n");
//	bsp_w25qxx_Read(ADDRESS , buff , SIZE);
//	HAL_Delay(100);
//	for(index = 0;index <SIZE;index++)
//	{
//		if((index %15 == 0) && (index!=0))
//		{
//			printf("\r\n");
//		}
//		printf("  %02x  ",buff[index]);
//		buff[index] = index % SIZE;
//	}

	//测试block的擦除功能。
//	printf("\r\n\r\nErase block test \r\n");
//	bsp_w25qxx_EraseBlock(ADDRESS);
//	HAL_Delay(100);
//	printf("Read data from chip\r\n");
//	bsp_w25qxx_Read(ADDRESS , buff , SIZE);
//	HAL_Delay(100);
//	for(index = 0;index <SIZE;index++)
//	{
//		if((index %15 == 0) && (index!=0))
//		{
//			printf("\r\n");
//		}
//		printf("  %02x  ",buff[index]);
//		buff[index] = index % SIZE;
//	}
//	printf("\r\nWrite data to chip\r\n");
//	memset(buff , 0x55 , SIZE);
//	SPI_FLASH_BufferWrite(buff ,ADDRESS ,  SIZE);
//	HAL_Delay(100);
//	memset(buff , 0 , SIZE);
//	printf("Get data from chip\r\n");
//	bsp_w25qxx_Read(ADDRESS , buff , SIZE);
//	HAL_Delay(100);
//	for(index = 0;index <SIZE;index++)
//	{
//		if((index %15 == 0) && (index!=0))
//		{
//			printf("\r\n");
//		}
//		printf("  %02x  ",buff[index]);
//		buff[index] = index % SIZE;
//	}


	//测试chip的擦除功能。
	printf("\r\n\r\nErase chip \r\n");
	bsp_w25qxx_EraseChip();
	printf("Read data from chip\r\n");
	bsp_w25qxx_Read(ADDRESS , buff , SIZE);
	for(index = 0;index <SIZE;index++)
	{
		if((index %15 == 0) && (index!=0))
		{
			printf("\r\n");
		}
		printf("  %02x  ",buff[index]);
		buff[index] = index % SIZE;
	}
//	printf("\r\nWrite data to chip\r\n");
//	bsp_w25qxx_Write(ADDRESS , buff , SIZE);
//	memset(buff , 0 , SIZE);
//	printf("Get data from chip\r\n");
//	bsp_w25qxx_Read(ADDRESS , buff , SIZE);
//	for(index = 0;index <SIZE;index++)
//	{
//		if((index %15 == 0) && (index!=0))
//		{
//			printf("\r\n");
//		}
//		printf("  %02x  ",buff[index]);
//		buff[index] = index % SIZE;
//	}
	

//	//进入mapped模式。
//	printf("\r\n\r\nMaped mode entered\r\n");
//	bsp_w25qxx_MemoryMapedEnter();
//	uint32_t add = 0x90000000;
//	printf("Read data from %x address\r\n",add+ADDRESS);
//	for(index = 0;index <SIZE;index++)
//	{
//		if((index %15 == 0) && (index!=0))
//		{
//			printf("\r\n");
//		}
//		printf("  %02x  ",*((uint8_t*)(add+ADDRESS + index)));
//	}
}
#endif



