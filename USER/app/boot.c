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


/* FatFs API����ֵ */
static const char * FR_Table[]= 
{
	"FR_OK���ɹ�",				                             /* (0) Succeeded */
	"FR_DISK_ERR���ײ����I/O�㷢��Ӳ������",			                 /* (1) A hard error occurred in the low level disk I/O layer */
	"FR_INT_ERR������ʧ��",				                     /* (2) Assertion failed */
	"FR_NOT_READY������������δ����",			             /* (3) The physical drive cannot work */
	"FR_NO_FILE��δ�ҵ��ļ�",				                 /* (4) Could not find the file */
	"FR_NO_PATH��δ�ҵ�·��",				                 /* (5) Could not find the path */
	"FR_INVALID_NAME����Ч���ļ���",		                     /* (6) The path name format is invalid */
	"FR_DENIED�����ڷ������ƻ�Ŀ¼���������ʱ��ܾ�",         /* (7) Access denied due to prohibited access or directory full */
	"FR_EXIST���ļ��Ѵ���",			                     /* (8) Access denied due to prohibited access */
	"FR_INVALID_OBJECT���ļ�/Ŀ¼������Ч",		         /* (9) The file/directory object is invalid */
	"FR_WRITE_PROTECTED��������������д����",		             /* (10) The physical drive is write protected */
	"FR_INVALID_DRIVE���߼�����������Ч",		                 /* (11) The logical drive number is invalid */
	"FR_NOT_ENABLED����û�й�����",			                 /* (12) The volume has no work area */
	"FR_NO_FILESYSTEM��û����Ч��FAT�ļ�ϵͳ",		             /* (13) There is no valid FAT volume */
	"FR_MKFS_ABORTED��f_mkfs()������������ֹ",	         /* (14) The f_mkfs() aborted due to any parameter error */
	"FR_TIMEOUT���ڹ涨ʱ�����޷���ȡ���ʾ��Ȩ��",		 /* (15) Could not get a grant to access the volume within defined period */
	"FR_LOCKED�������ļ�������ԣ��������ܾ�",				 /* (16) The operation is rejected according to the file sharing policy */
	"FR_NOT_ENOUGH_CORE���޷�����LFN����������",		     /* (17) LFN working buffer could not be allocated */
	"FR_TOO_MANY_OPEN_FILES����ǰ�򿪵��ļ�������_FS_SHARE", /* (18) Number of open files > _FS_SHARE */
	"FR_INVALID_PARAMETER������������Ч"	                     /* (19) Given parameter is invalid */
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
			printf("��1 - ��ʾSD����Ŀ¼�µ��ļ���Ŀ¼\r\n");
				view_root_dir();
			 break;
			case 2:
			printf("��2 - ����Ӧ�ó���, У��CRC��\r\n");
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
*	�� �� ��: DispMenu
*	����˵��: ��ʾ�����˵�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void disp_menu(void)
{
	printf("��ѡ�����ѡ��:\r\n");
	printf("1 - ��ʾSD����Ŀ¼�µ��ļ���Ŀ¼\r\n");
	printf("2 - ����Ӧ�ó���, У��CRC��\r\n");
}

static void bsp_deinit(void)
{
	uint8_t i;
	/* �ر�ȫ���ж� */
	DISABLE_INT(); 

	/* ��λ����ʱ�ӵ�Ĭ��״̬��ʹ��HSIʱ�� */
	HAL_RCC_DeInit();

	/* �ر�SysTickʱ�Ӳ���λ��Ĭ��ֵ */
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	/* �ر������жϣ���������жϱ�־ */
	for (i = 0; i < 8; i++)
	{
		NVIC->ICER[i]=0xFFFFFFFF;
		NVIC->ICPR[i]=0xFFFFFFFF;
	}	

	/* ʹ��ȫ���ж� */
	ENABLE_INT();

}
/*
*********************************************************************************************************
*	�� �� ��: JumpToApp
*	����˵��: ��ת��Ӧ�ó���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void jump_to_app(void)
{
    app_jump app;
	
    bsp_deinit();

    /* ��ת��Ӧ�ó�����ڵ�ַ��MSPָ���ַ+4�ֽڴ�Ϊ��λ������ַ */
	// ���Ӧ�ó����Ƿ�λ��Flash�У�STM32��Flash��ʼ��ַΪ0x08xxxxxx
	if ((*(uint32_t *)(APP_ADDR + 4) & 0xFF000000) != 0x08000000)
		return;
	app = (app_jump)(*(uint32_t *)(APP_ADDR + 4));

	// ���ջָ���Ƿ�ָ��RAM��STM32H7��AXI SRAM��ʼ��ַΪ0x24000000
	if ((*(uint32_t *)APP_ADDR & 0xFF000000) != 0x24000000)
		return;
	__set_MSP(*(uint32_t *)APP_ADDR);

    /* ��ʹ��RTOS����Ҫ����Ϊ��Ȩģʽ��ʹ��MSPָ�� */
	__set_CONTROL(0);
	
    app();
	
    /* ��ת�ɹ��󲻻�ִ�е������ִ�е�����˵����תʧ�� */
	while (1)
	{

	}
}
/*
*********************************************************************************************************
*	�� �� ��: BootHexCrcVeriy
*	����˵��: Ӧ�ó���CRCУ��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void boot_hex_crc(void)
{
    CRC_HandleTypeDef   CrcHandle = {0};
	
	/* ��ȡbin�ļ���CRC */
	uwExpectedCRCValue  = *(__IO uint32_t *)(APP_ADDR + uwAppSize - 4);
	
	/* ��ʼ��Ӳ��CRC */
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

	/* ����Ӧ�ó����Ӳ��CRC */
	uwCRCValue = HAL_CRC_Calculate(&CrcHandle, (uint32_t *)(APP_ADDR), uwAppSize/4 - 1);
	
	printf("ʵ��APP����CRCУ��ֵ = 0x%x\r\n", uwExpectedCRCValue);
	printf("����APP����CRCУ��ֵ = 0x%x\r\n", uwCRCValue);	
	
	if (uwCRCValue != uwExpectedCRCValue)
	{
		printf("У��ʧ��\r\n");
        // Error_Handler();		
	}
	else
	{
		printf("У��ͨ����׼����ת��APP\r\n");
	}
	
	printf("=================================================\r\n");

}
/*
*********************************************************************************************************
*	�� �� ��: LoadFirmware
*	����˵��: ���ع̼�
*	��    ��: ��
*	�� �� ֵ: ��
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

	result = f_mount(&fs, DiskPath , 1);	/* �����߼������� */
    if (result != FR_OK)
	{
		printf("�����ļ�ϵͳʧ�� (%s)\r\n", FR_Table[result]);
        return;
	}

	//=============================����app===========================================
	sprintf(file_path,"%sapp.bin",DiskPath);
    result = f_open(&file,file_path,FA_OPEN_EXISTING | FA_READ);
    if (result != FR_OK)
	{
		f_close(&file);
		/* ж���ļ�ϵͳ */
		f_mount(NULL, DiskPath, 0);
		printf("δ�ҵ��ļ� : app.bin\r\n");
        return;
	}

	f_stat(file_path,&fno);
    uwAppSize = fno.fsize;
    printf("APP�����С��%d\r\n", (int)fno.fsize);
//    f_lseek(&file,28);
//    f_read(&file,&ver,sizeof(ver),&bw);
//    f_lseek(&file,0);
//    printf("APP����汾��V%X.%02X\r\n", ver >> 8, ver & 0xFF);

	SectorCount = uwAppSize / (128 * 1024);
    SectorRemain = uwAppSize % (128 * 1024);

	for(i=0;i<SectorCount;i++)
    {
        printf("��ʼ�������� = %08x\r\n", APP_ADDR + i*128*1024);
		bsp_EraseCpuFlash((uint32_t)(APP_ADDR + i*128*1024));
    }
    if(SectorRemain)
    {
        printf("��ʼ����ʣ������ = %08x\r\n", APP_ADDR + i*128*1024);
		bsp_EraseCpuFlash((uint32_t)(APP_ADDR + i*128*1024));
    }

	while(1)
    {
        result = f_read(&file,tempbuf,sizeof(tempbuf),&bw);

		if ((result != FR_OK)||bw == 0)
		{
			printf("APP����������\r\n");
			printf("=================================================\r\n");
			break;
		}

        /* ����ֵΪ0��ʾд��ɹ� */
		TotalSize += bw;
		ucState = bsp_WriteCpuFlash((uint32_t)(APP_ADDR + Count*sizeof(tempbuf)),  (uint8_t *)tempbuf, bw);
		
		/* ����ֵΪ0��ʾд��ɹ� */
		if(ucState != 0)
		{
			printf("д��ʧ��\r\n");
			break;
		}
		
        /* ��ʾ���ؽ��� */
		Count = Count + 1;
		FinishPecent = (float)(TotalSize) / fno.fsize;
		printf("��ǰ����:%02d%%\r\n", (uint8_t)(FinishPecent*100));
    }
	FinishPecent = 0;
	TotalSize = 0;
	Count = 0;
	f_close(&file);
	
	//=============================����flash===========================================
	sprintf(file_path,"%sflash.bin",DiskPath);
    result = f_open(&file,file_path,FA_OPEN_EXISTING | FA_READ);
    if (result != FR_OK)
	{
		f_close(&file);
		/* ж���ļ�ϵͳ */
		f_mount(NULL, DiskPath, 0);
		printf("δ�ҵ��ļ� : flash.bin\r\n");
        return;
	}

	f_stat(file_path,&fno);
    uwAppSize = fno.fsize;
    printf("Flash�ļ���С��%d\r\n", (int)fno.fsize);

	SectorCount = uwAppSize / (4 * 1024);
    SectorRemain = uwAppSize % (4 * 1024);

    for(i=0;i<SectorCount;i++)
    {
		printf("��ʼ�������� = %08x\r\n", i*4*1024);
		bsp_w25qxx_EraseSector(((uint32_t)(i*4*1024)));
    }
    if(SectorRemain)
    {
		printf("��ʼ����ʣ������ = %08x\r\n", i*4*1024);
		bsp_w25qxx_EraseSector(((uint32_t)(i*4*1024)));
    }

	while(1)
    {
        result = f_read(&file,tempbuf,sizeof(tempbuf),&bw);

		if ((result != FR_OK)||bw == 0)
		{
			printf("Flash�ļ��������\r\n");
			printf("=================================================\r\n");
			break;
		}

        /* ����ֵΪ0��ʾд��ɹ� */
		TotalSize += bw;
		SPI_FLASH_BufferWrite(((uint8_t *)tempbuf),(uint32_t)(Count*sizeof(tempbuf)),bw);
 
        /* ��ʾ���ؽ��� */
		Count = Count + 1;
		FinishPecent = (float)(TotalSize) / fno.fsize;
		printf("��ǰ����:%02d%%\r\n", (uint8_t)(FinishPecent*100));
    }

	f_close(&file);
	
	/* ж���ļ�ϵͳ */
	f_mount(NULL, DiskPath, 0);
	
	
	/* ���Flashӳ�� */
}

/*
*********************************************************************************************************
*	�� �� ��: ViewRootDir
*	����˵��: ��ʾSD����Ŀ¼�µ��ļ���Ŀ¼
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void view_root_dir(void)
{
    DIR DirInf;
    FILINFO FileInf;
    uint8_t result;
	uint8_t cnt = 0;
	

    
 	/* �����ļ�ϵͳ */
	result = f_mount(&fs, DiskPath , 1);	/* �����߼������� */

	if (result != FR_OK)
	{
		printf("�����ļ�ϵͳʧ�� (%s)\r\n", FR_Table[result]);
	}

	/* ��Ŀ¼ */
	result = f_opendir(&DirInf, DiskPath); /* �Ӹ�Ŀ¼��ʼ��� */
	if (result != FR_OK)
	{
		printf("��Ŀ¼ʧ��  (%s)\r\n", FR_Table[result]);
		return;
	}

	printf("����        |  �ļ���С |  �ļ��� |  �ļ�����\r\n");
	for (cnt = 0; ;cnt++)
	{
		result = f_readdir(&DirInf, &FileInf); 		/* ��ȡĿ¼�����ĩβ���˳� */
		if (result != FR_OK || FileInf.fname[0] == 0)
		{
			break;
		}

		if (FileInf.fname[0] == '.')
		{
			continue;
		}

		/* �ж����ļ�����Ŀ¼ */
		if (FileInf.fattrib & AM_DIR)
		{
			printf("(0x%02d)Ŀ¼  ", FileInf.fattrib);
		}
		else
		{
			printf("(0x%02d)�ļ�  ", FileInf.fattrib);
		}

		f_stat(FileInf.fname, &fno);
		
		/* ��ӡ�ļ���С����λ�ֽ� */
		printf(" %10d", (int)fno.fsize);


		printf("  %s\r\n", (char *)FileInf.fname);	/* �ļ��� */
	}

    
	/* ж���ļ�ϵͳ */
	 f_mount(NULL,DiskPath, 1);
}