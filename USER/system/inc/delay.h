#ifndef __DELAY_H
#define __DELAY_H 

#include "main.h"

void delay_init(u8 SYSCLK);
void delay_ms(u16 nms);
void delay_us(u32 nus); 

//#define delay_ms(x) osDelay(x)

#endif





























