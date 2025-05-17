/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "mdma.h"
#include "memorymap.h"
#include "quadspi.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_usb_msc.h"
#include "bsp_flash_qspi.h"
#include "ff.h"
#include "boot.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*����ΪNormal��cache������Ϊ write-back����������cache��
 *�ں��ʵ�ʱ��(��cache���Ծ����������ǿ�Ƹ���)�����ݸ��µ���Ӧ��SRAM�ռ�
 *�ر�ע�⣺���Ҫ�����������£�д֮��ҪSCB_CleanDCache��������ʱҪSCB_InvalidateDCache
 */
#define MPU_Normal_WB        0x00


/*����ΪNormal��cache������Ϊ write-back����������cache��
 *�ں��ʵ�ʱ��(��cache���Ծ����������ǿ�Ƹ���)�����ݸ��µ���Ӧ��SRAM�ռ�
 *�ر�ע�⣺���Ҫ�����������£�д֮��ҪSCB_CleanDCache��������ʱҪSCB_InvalidateDCache
 */
#define MPU_Normal_WBWARA    0x01  //�ⲿ���ڲ�д����д�����


/*����Ϊ normal��cache������Ϊ Write-through��������cache��ͬʱ��
 *������ͬʱд����Ӧ�������ַ�ռ�
 *�ر�ע�⣺���Ҫ�����������£�����ֱ�����ڴ�д���ݣ���������ʱҪSCB_InvalidateDCache
 */
#define MPU_Normal_WT         0x02


/*����Ϊ normal�����ù���,���û���
 */
#define MPU_Normal_NonCache   0x03


/*����Ϊ Device������������Ч�����ù���,���û���
 */
#define MPU_Device_NonCache   0x04

/**
  * @brief  ����MPU�������Ժʹ�С�Ĵ���ֵ
    * @param  Region            MPU��������ȡֵ��Χ��0��7��
    * @param  AccessPermission  ���ݷ���Ȩ�ޣ�ȡֵ��Χ��MPU_NO_ACCESS��MPU_PRIV_RO_URO��
    * @param  TypeExtField      �������� Cache ���ԣ�����ȡֵ��0��1��2����һ��ֻ��0��1
    *
  * @param  Address             MPU�����������ַ���ر�ע�����õ�Address��Ҫ��Size����
  * @param  Size                MPU���������С,����ȡֵ��MPU_1KB��MPU_4KB ...MPU_512MB��
    * @param  IsShareable       �����Ĵ洢�ռ��Ƿ���Թ���1=������0=��ֹ����
  * @param  IsCacheable         �����Ĵ洢�ռ��Ƿ���Ի��棬1=�����棬0=��ֹ���档
  * @param  IsBufferable        ʹ��Cache֮�󣬲�����write-through����write-back(bufferable)
    *                           1=�����壬����д��write-back����0=��ֹ���壬��ֱд��write-through����
  * @retval None
  */
static void BSP_MPU_ConfigRegion(uint8_t    Number,
                                 uint8_t    TypeExtField,
                                 uint32_t   Address,
                                 uint32_t   Size,
                                 uint8_t    IsBufferable,
                                 uint8_t    IsCacheable)
{
    MPU_Region_InitTypeDef MPU_InitStruct;

    /* ����MPU */
    HAL_MPU_Disable();

    /* ����MPU����*/
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress = Address;                      //�������ַ��
    MPU_InitStruct.Size = Size;                                //Ҫ���õ�����������С��
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;  //���ݷ���Ȩ�������������û�����Ȩģʽ�Ķ�/д����Ȩ�ޡ�
    MPU_InitStruct.IsBufferable = IsBufferable;                //�����ǿɻ���ģ���ʹ�û�д���档 �ɻ��浫���ɻ��������ʹ��ֱд���ԡ�WB
    MPU_InitStruct.IsCacheable = IsCacheable;                  //�����Ƿ�ɻ���ģ�����ֵ�Ƿ���Ա����ڻ����С�//M7 �ں�ֻҪ������ Cache��read allocate ���ǿ�����
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;     //�����Ƿ�����ڶ������������֮�乲��H7 ��Ӧ�ñʼǶ���������ǿ������������ͬ�ڹر� Cache
    MPU_InitStruct.Number = Number;                            //����š�
    MPU_InitStruct.TypeExtField = TypeExtField;                //������չ�ֶΣ������������ڴ�������͡�
    MPU_InitStruct.SubRegionDisable = 0x00;                    //����������ֶΡ�
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;//ָ����ʽ���λ��0=����ָ����ʣ�1=��ָֹ�����

    HAL_MPU_ConfigRegion(&MPU_InitStruct);
    /* ����MPU */
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT); //��ʾ��ֹ�˱������������κ�δʹ�� MPU �������������ڴ��쳣 MemFault


}

void cpu_mpu_config(uint8_t Region, uint8_t Mode, uint32_t Address, uint32_t Size)
{
    switch (Mode)
    {
    case MPU_Normal_WB:
        /*write back,no write allocate */
        /* �����ڴ�ΪNormal����,���ù���, ��дģʽ����д���ȡ����*/
        BSP_MPU_ConfigRegion(Region, MPU_TEX_LEVEL0, Address, Size, MPU_ACCESS_BUFFERABLE, MPU_ACCESS_CACHEABLE);
        break;

    case MPU_Normal_WBWARA:
        /*write back,write and read allocate */
        /* �����ڴ�ΪNormal����,���ù���, ��дģʽ��д���ȡ����*/
        BSP_MPU_ConfigRegion(Region, MPU_TEX_LEVEL1, Address, Size, MPU_ACCESS_BUFFERABLE, MPU_ACCESS_CACHEABLE);
        break;

    case MPU_Normal_WT:
        /*write through,no write allocate */
        /* �����ڴ�ΪNormal����,���ù���, ֱдģʽ*/
        BSP_MPU_ConfigRegion(Region, MPU_TEX_LEVEL0, Address, Size, MPU_ACCESS_NOT_BUFFERABLE, MPU_ACCESS_CACHEABLE);
        break;

    case MPU_Normal_NonCache:
        /* �����ڴ�ΪNormal����,���ù������û���ģʽ*/
        BSP_MPU_ConfigRegion(Region, MPU_TEX_LEVEL1, Address, Size, MPU_ACCESS_NOT_BUFFERABLE, MPU_ACCESS_NOT_CACHEABLE);
        break;
    }
}

/*
*********************************************************************************************************
*	�� �� ��: fputc
*	����˵��: �ض���putc��������������ʹ��printf�����Ӵ���1��ӡ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
int fputc(int ch, FILE *f)
{

	HAL_UART_Transmit(&huart1,(uint8_t *)&ch,1,100);
	
	return ch;
}

/*
*********************************************************************************************************
*	�� �� ��: fgetc
*	����˵��: �ض���getc��������������ʹ��getchar�����Ӵ���1��������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
int fgetc(FILE *f)
{
	uint8_t ucData,ret;
	
	ret = HAL_UART_Receive(&huart1,(uint8_t *)&ucData,1,10);
	if(ret)
		ucData = 0;
	return ucData;
}
const u8 TEXT_Buffer[]={"STM32 FLASH TEST"};
#define TEXT_LENTH sizeof(TEXT_Buffer)	 		  	//���鳤��	
#define SIZE TEXT_LENTH/4+((TEXT_LENTH%4)?1:0)

#define FLASH_SAVE_ADDR  0X08020000 	//����FLASH �����ַ(����Ϊ4�ı���������������,Ҫ���ڱ�������ռ�õ�������.

uint8_t flash_fatfs_test(void)
{
	uint8_t i = 0;
    uint16_t BufferSize = 0;
    FIL	MyFile;			// �ļ�����
    UINT 	MyFile_Num;		//	���ݳ���
    BYTE 	MyFile_WriteBuffer[] = "STM32H7B0 SD�� �ļ�ϵͳ����";	//Ҫд�������
    BYTE 	MyFile_ReadBuffer[4096];	//Ҫ����������
    uint8_t MyFile_Res;    /* Return value for SD */
    printf("-------------FatFs �ļ�������д�����---------------\r\n");

    /* �����ļ�ϵͳ */
    FATFS fs;
    MyFile_Res = f_mount(&fs, "1:", 1); 
    if (MyFile_Res != FR_OK) {
        return MyFile_Res;
    }

    MyFile_Res = f_open(&MyFile,"1:test.txt",FA_CREATE_ALWAYS | FA_WRITE);	//���ļ������������򴴽����ļ�
    if(MyFile_Res == FR_OK)
    {
        printf("�ļ���/�����ɹ���׼��д������...\r\n");

        MyFile_Res = f_write(&MyFile,MyFile_WriteBuffer,sizeof(MyFile_WriteBuffer),&MyFile_Num);	//���ļ�д������
        if (MyFile_Res == FR_OK)
        {
            printf("д��ɹ���д������Ϊ��\r\n");
            printf("%s\r\n",MyFile_WriteBuffer);
        }
        else
        {
            printf("�ļ�д��ʧ�ܣ�����SD�������¸�ʽ��!\r\n");
            f_close(&MyFile);	  //�ر��ļ�
            return ERROR;
        }
        f_close(&MyFile);	  //�ر��ļ�
    }
    else
    {
        printf("�޷���/�����ļ�������SD�������¸�ʽ��!\r\n");
        f_close(&MyFile);	  //�ر��ļ�
        return ERROR;
    }

    printf("-------------FatFs �ļ���ȡ����---------------\r\n");

    BufferSize = sizeof(MyFile_WriteBuffer)/sizeof(BYTE);									// ����д������ݳ���
    MyFile_Res = f_open(&MyFile,"1:test.txt",FA_OPEN_EXISTING | FA_READ);	//���ļ������������򴴽����ļ�
    MyFile_Res = f_read(&MyFile,MyFile_ReadBuffer,BufferSize,&MyFile_Num);			// ��ȡ�ļ�
    if(MyFile_Res == FR_OK)
    {
        printf("�ļ���ȡ�ɹ�������У������...\r\n");

        for(i=0; i<BufferSize; i++)
        {
            if(MyFile_WriteBuffer[i] != MyFile_ReadBuffer[i])		// У������
            {
                printf("У��ʧ�ܣ�����SD�������¸�ʽ��!\r\n");
                f_close(&MyFile);	  //�ر��ļ�
                return ERROR;
            }
        }
        printf("У��ɹ�������������Ϊ��\r\n");
        printf("%s\r\n",MyFile_ReadBuffer);
    }
    else
    {
        printf("�޷���ȡ�ļ�������SD�������¸�ʽ��!\r\n");
        f_close(&MyFile);	  //�ر��ļ�
        return ERROR;
    }

    f_close(&MyFile);	  //�ر��ļ�
    f_mount(NULL, "", 0);
    return SUCCESS;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	cpu_mpu_config(0, MPU_Normal_NonCache, 0x24080000-0x4000, MPU_REGION_SIZE_64KB);
	cpu_mpu_config(1, MPU_Normal_NonCache, 0x24000000, MPU_REGION_SIZE_512KB);
	 /* Enable I-Cache */
	SCB_EnableICache();
	/* Enable D-Cache */
	SCB_EnableDCache();
	
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_MDMA_Init();
  MX_USART1_UART_Init();
  MX_QUADSPI_Init();
  /* USER CODE BEGIN 2 */

  bsp_w25qxx_Init();

//	bsp_w25qxx_test();
	
  msc_ram_init(0,USB_OTG_FS_PERIPH_BASE); 

  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  boot_main();
	  
	  
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 160;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  RCC_OscInitStruct.PLL.PLLR = 8;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
