#include "delay.h"


extern TIM_HandleTypeDef        htim4;


//延时nus
//nus为要延时的us数.	
//nus:0~190887435(最大值即2^32/fac_us@fac_us=22.5)	 
void delay_us(uint32_t nus)
{		

	uint32_t told = __HAL_TIM_GET_COUNTER(&htim4); // timer4的计数值
	uint32_t tnow;
	
	uint32_t load =  __HAL_TIM_GET_AUTORELOAD(&htim4); // timer4的auto-reload值
	
	/* LOAD+1个时钟对应1ms
	 * n us对应 n*(load+1)/1000个时钟
   */
	uint32_t ticks = nus*(load+1)/1000;
	
	uint32_t cnt = 0;
	
	while (1)
	{
		tnow = __HAL_TIM_GET_COUNTER(&htim4);
		if (tnow >= told)
			cnt += tnow - told;
		else
			cnt += load + 1 - told + tnow;
		
		told = tnow;
		if (cnt >= ticks)
			break;
	}	
}

//延时nms
//nms:要延时的ms数
void delay_ms(uint16_t nms)
{
	uint32_t i;
	for(i=0;i<nms;i++) delay_us(1000);
}
















