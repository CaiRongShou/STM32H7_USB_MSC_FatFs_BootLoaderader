#include "boot.h"
#include "ff.h"
#include <stdio.h>
#include "bsp_usb_msc.h"
#include "bsp_cpu_flash.h"
#include "bsp_flash_qspi.h"

#include "quadspi.h"
#include "usart.h"
#include "usb_otg.h"

#define APP_ADDR  0x08020000    
#define BOOT_KEY  HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0)  


FATFS fs;
FILINFO fno;
FIL file;
char DiskPath[4] = {"1:\\"};                      

__IO uint32_t uwCRCValue;
__IO uint32_t uwExpectedCRCValue;
__IO uint32_t uwAppSize;

uint8_t tempbuf[4096];  

typedef void (*app_jump)(void);

static void disp_menu(void);
static void bsp_deinit(void);
static void jump_to_app(void);
static void boot_load_firmware(void);
static void boot_hex_crc(void);
static void view_root_dir(void);


/* FatFs API返回值 */
static const char * FR_Table[]= 
{
	"FR_OK：成功",				                             /* (0) Succeeded */
	"FR_DISK_ERR：底层磁盘I/O层发生硬件错误",			                 /* (1) A hard error occurred in the low level disk I/O layer */
	"FR_INT_ERR：断言失败",				                     /* (2) Assertion failed */
	"FR_NOT_READY：物理驱动器未就绪",			             /* (3) The physical drive cannot work */
	"FR_NO_FILE：未找到文件",				                 /* (4) Could not find the file */
	"FR_NO_PATH：未找到路径",				                 /* (5) Could not find the path */
	"FR_INVALID_NAME：无效的文件名",		                     /* (6) The path name format is invalid */
	"FR_DENIED：由于访问限制或目录已满，访问被拒绝",         /* (7) Access denied due to prohibited access or directory full */
	"FR_EXIST：文件已存在",			                     /* (8) Access denied due to prohibited access */
	"FR_INVALID_OBJECT：文件/目录对象无效",		         /* (9) The file/directory object is invalid */
	"FR_WRITE_PROTECTED：物理驱动器被写保护",		             /* (10) The physical drive is write protected */
	"FR_INVALID_DRIVE：逻辑驱动器号无效",		                 /* (11) The logical drive number is invalid */
	"FR_NOT_ENABLED：卷没有工作区",			                 /* (12) The volume has no work area */
	"FR_NO_FILESYSTEM：没有有效的FAT文件系统",		             /* (13) There is no valid FAT volume */
	"FR_MKFS_ABORTED：f_mkfs()因参数错误而中止",	         /* (14) The f_mkfs() aborted due to any parameter error */
	"FR_TIMEOUT：在规定时间内无法获取访问卷的权限",		 /* (15) Could not get a grant to access the volume within defined period */
	"FR_LOCKED：根据文件共享策略，操作被拒绝",				 /* (16) The operation is rejected according to the file sharing policy */
	"FR_NOT_ENOUGH_CORE：无法分配LFN工作缓冲区",		     /* (17) LFN working buffer could not be allocated */
	"FR_TOO_MANY_OPEN_FILES：当前打开的文件数超过_FS_SHARE", /* (18) Number of open files > _FS_SHARE */
	"FR_INVALID_PARAMETER：给定参数无效"	                     /* (19) Given parameter is invalid */
};

//__asm void MSP_SP(uint32_t addr)
//{
//	MSR msp ,r0
//	BX r14
//	
//}

void boot_main(void)
{
	uint8_t temp = 0;
	if(!BOOT_KEY) 
		jump_to_app();
	
    disp_menu();
    while (1)
    {
		msc_ram_polling(0);
		temp = getchar();
        switch (temp)
        {
			case 0:
				break;
			case 1:
			printf("按1 - 显示SD卡根目录下的文件和目录\r\n");
				view_root_dir();
			 break;
			case 2:
			printf("按2 - 加载应用程序, 校验CRC码\r\n");
			boot_load_firmware();
			//boot_hex_crc();
			jump_to_app();
			break;
			default:
				disp_menu();
			break;
        }
    }
    
}


/*
*********************************************************************************************************
*	函 数 名: DispMenu
*	功能说明: 显示操作菜单
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void disp_menu(void)
{
	printf("请选择操作选项:\r\n");
	printf("1 - 显示SD卡根目录下的文件和目录\r\n");
	printf("2 - 加载应用程序, 校验CRC码\r\n");
}

static void bsp_deinit(void)
{
	uint8_t i;
	/* 关闭全局中断 */
	DISABLE_INT(); 

	/* 复位所有时钟到默认状态，使用HSI时钟 */
	HAL_RCC_DeInit();

	/* 关闭SysTick时钟并复位到默认值 */
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	/* 关闭所有中断，清除所有中断标志 */
	for (i = 0; i < 8; i++)
	{
		NVIC->ICER[i]=0xFFFFFFFF;
		NVIC->ICPR[i]=0xFFFFFFFF;
	}	

	/* 使能全局中断 */
	ENABLE_INT();

}
/*
*********************************************************************************************************
*	函 数 名: JumpToApp
*	功能说明: 跳转至应用程序
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void jump_to_app(void)
{
    app_jump app;
	
    bsp_deinit();

    /* 跳转至应用程序入口地址，MSP指针地址+4字节处为复位向量地址 */
	// 检查应用程序是否位于Flash中，STM32的Flash起始地址为0x08xxxxxx
	if ((*(uint32_t *)(APP_ADDR + 4) & 0xFF000000) != 0x08000000)
		return;
	app = (app_jump)(*(uint32_t *)(APP_ADDR + 4));

	// 检查栈指针是否指向RAM，STM32H7的AXI SRAM起始地址为0x24000000
	if ((*(uint32_t *)APP_ADDR & 0xFF000000) != 0x24000000)
		return;
	__set_MSP(*(uint32_t *)APP_ADDR);

    /* 若使用RTOS，需要设置为特权模式，使用MSP指针 */
	__set_CONTROL(0);
	
    app();
	
    /* 跳转成功后不会执行到这里，若执行到这里说明跳转失败 */
	while (1)
	{

	}
}
/*
*********************************************************************************************************
*	函 数 名: BootHexCrcVeriy
*	功能说明: 应用程序CRC校验
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void boot_hex_crc(void)
{
    CRC_HandleTypeDef   CrcHandle = {0};
	
	/* 获取bin文件的CRC */
	uwExpectedCRCValue  = *(__IO uint32_t *)(APP_ADDR + uwAppSize - 4);
	
	/* 初始化硬件CRC */
	__HAL_RCC_CRC_CLK_ENABLE();      
	
	CrcHandle.Instance = CRC;
	CrcHandle.Init.DefaultPolynomialUse    = DEFAULT_POLYNOMIAL_ENABLE;
	CrcHandle.Init.DefaultInitValueUse     = DEFAULT_INIT_VALUE_ENABLE;
	CrcHandle.Init.InputDataInversionMode  = CRC_INPUTDATA_INVERSION_NONE;
	CrcHandle.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
	CrcHandle.InputDataFormat              = CRC_INPUTDATA_FORMAT_WORDS;

	if (HAL_CRC_Init(&CrcHandle) != HAL_OK)
	{
        Error_Handler();
	}

	/* 计算应用程序的硬件CRC */
	uwCRCValue = HAL_CRC_Calculate(&CrcHandle, (uint32_t *)(APP_ADDR), uwAppSize/4 - 1);
	
	printf("实际APP程序CRC校验值 = 0x%x\r\n", uwExpectedCRCValue);
	printf("计算APP程序CRC校验值 = 0x%x\r\n", uwCRCValue);	
	
	if (uwCRCValue != uwExpectedCRCValue)
	{
		printf("校验失败\r\n");
        // Error_Handler();		
	}
	else
	{
		printf("校验通过，准备跳转至APP\r\n");
	}
	
	printf("=================================================\r\n");

}
/*
*********************************************************************************************************
*	函 数 名: LoadFirmware
*	功能说明: 加载固件
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void boot_load_firmware(void)
{
    char file_path[64];
    uint8_t result;
    uint32_t bw;
    uint32_t ver;
    uint8_t SectorCount = 0;
	uint8_t SectorRemain = 0;
    uint8_t i;
    uint32_t Count = 0;
    uint32_t TotalSize = 0;
	uint8_t ucState;
    float FinishPecent;

	result = f_mount(&fs, DiskPath , 1);	/* 挂载逻辑驱动器 */
    if (result != FR_OK)
	{
		printf("挂载文件系统失败 (%s)\r\n", FR_Table[result]);
        return;
	}

	//=============================加载app===========================================
	sprintf(file_path,"%sapp.bin",DiskPath);
    result = f_open(&file,file_path,FA_OPEN_EXISTING | FA_READ);
    if (result != FR_OK)
	{
		f_close(&file);
		/* 卸载文件系统 */
		f_mount(NULL, DiskPath, 0);
		printf("未找到文件 : app.bin\r\n");
        return;
	}

	f_stat(file_path,&fno);
    uwAppSize = fno.fsize;
    printf("APP程序大小：%d\r\n", (int)fno.fsize);
//    f_lseek(&file,28);
//    f_read(&file,&ver,sizeof(ver),&bw);
//    f_lseek(&file,0);
//    printf("APP程序版本：V%X.%02X\r\n", ver >> 8, ver & 0xFF);

	SectorCount = uwAppSize / (128 * 1024);
    SectorRemain = uwAppSize % (128 * 1024);

	for(i=0;i<SectorCount;i++)
    {
        printf("开始擦除扇区 = %08x\r\n", APP_ADDR + i*128*1024);
		bsp_EraseCpuFlash((uint32_t)(APP_ADDR + i*128*1024));
    }
    if(SectorRemain)
    {
        printf("开始擦除剩余扇区 = %08x\r\n", APP_ADDR + i*128*1024);
		bsp_EraseCpuFlash((uint32_t)(APP_ADDR + i*128*1024));
    }

	while(1)
    {
        result = f_read(&file,tempbuf,sizeof(tempbuf),&bw);

		if ((result != FR_OK)||bw == 0)
		{
			printf("APP程序加载完成\r\n");
			printf("=================================================\r\n");
			break;
		}

        /* 返回值为0表示写入成功 */
		TotalSize += bw;
		ucState = bsp_WriteCpuFlash((uint32_t)(APP_ADDR + Count*sizeof(tempbuf)),  (uint8_t *)tempbuf, bw);
		
		/* 返回值为0表示写入成功 */
		if(ucState != 0)
		{
			printf("写入失败\r\n");
			break;
		}
		
        /* 显示加载进度 */
		Count = Count + 1;
		FinishPecent = (float)(TotalSize) / fno.fsize;
		printf("当前进度:%02d%%\r\n", (uint8_t)(FinishPecent*100));
    }
	FinishPecent = 0;
	TotalSize = 0;
	Count = 0;
	f_close(&file);
	
	//=============================加载flash===========================================
	sprintf(file_path,"%sflash.bin",DiskPath);
    result = f_open(&file,file_path,FA_OPEN_EXISTING | FA_READ);
    if (result != FR_OK)
	{
		f_close(&file);
		/* 卸载文件系统 */
		f_mount(NULL, DiskPath, 0);
		printf("未找到文件 : flash.bin\r\n");
        return;
	}

	f_stat(file_path,&fno);
    uwAppSize = fno.fsize;
    printf("Flash文件大小：%d\r\n", (int)fno.fsize);

	SectorCount = uwAppSize / (4 * 1024);
    SectorRemain = uwAppSize % (4 * 1024);

    for(i=0;i<SectorCount;i++)
    {
		printf("开始擦除扇区 = %08x\r\n", i*4*1024);
		bsp_w25qxx_EraseSector(((uint32_t)(i*4*1024)));
    }
    if(SectorRemain)
    {
		printf("开始擦除剩余扇区 = %08x\r\n", i*4*1024);
		bsp_w25qxx_EraseSector(((uint32_t)(i*4*1024)));
    }

	while(1)
    {
        result = f_read(&file,tempbuf,sizeof(tempbuf),&bw);

		if ((result != FR_OK)||bw == 0)
		{
			printf("Flash文件加载完成\r\n");
			printf("=================================================\r\n");
			break;
		}

        /* 返回值为0表示写入成功 */
		TotalSize += bw;
		SPI_FLASH_BufferWrite(((uint8_t *)tempbuf),(uint32_t)(Count*sizeof(tempbuf)),bw);
 
        /* 显示加载进度 */
		Count = Count + 1;
		FinishPecent = (float)(TotalSize) / fno.fsize;
		printf("当前进度:%02d%%\r\n", (uint8_t)(FinishPecent*100));
    }

	f_close(&file);
	
	/* 卸载文件系统 */
	f_mount(NULL, DiskPath, 0);
	
	
	/* 完成Flash映射 */
}

/*
*********************************************************************************************************
*	函 数 名: ViewRootDir
*	功能说明: 显示SD卡根目录下的文件和目录
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void view_root_dir(void)
{
    DIR DirInf;
    FILINFO FileInf;
    uint8_t result;
	uint8_t cnt = 0;
	

    
 	/* 挂载文件系统 */
	result = f_mount(&fs, DiskPath , 1);	/* 挂载逻辑驱动器 */

	if (result != FR_OK)
	{
		printf("挂载文件系统失败 (%s)\r\n", FR_Table[result]);
	}

	/* 打开目录 */
	result = f_opendir(&DirInf, DiskPath); /* 从根目录开始浏览 */
	if (result != FR_OK)
	{
		printf("打开目录失败  (%s)\r\n", FR_Table[result]);
		return;
	}

	printf("属性        |  文件大小 |  文件名 |  文件类型\r\n");
	for (cnt = 0; ;cnt++)
	{
		result = f_readdir(&DirInf, &FileInf); 		/* 获取目录项，读到末尾则退出 */
		if (result != FR_OK || FileInf.fname[0] == 0)
		{
			break;
		}

		if (FileInf.fname[0] == '.')
		{
			continue;
		}

		/* 判断是文件还是目录 */
		if (FileInf.fattrib & AM_DIR)
		{
			printf("(0x%02d)目录  ", FileInf.fattrib);
		}
		else
		{
			printf("(0x%02d)文件  ", FileInf.fattrib);
		}

		f_stat(FileInf.fname, &fno);
		
		/* 打印文件大小，单位字节 */
		printf(" %10d", (int)fno.fsize);


		printf("  %s\r\n", (char *)FileInf.fname);	/* 文件名 */
	}

    
	/* 卸载文件系统 */
	 f_mount(NULL,DiskPath, 1);
}