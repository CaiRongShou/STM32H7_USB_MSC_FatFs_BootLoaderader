#ifndef __BSP_USB_MSC_H_
#define __BSP_USB_MSC_H_
#include "usbd_core.h"
#include "usbd_msc.h"

void usb_dc_low_level_deinit(uint8_t busid);
void msc_ram_init(uint8_t busid, uintptr_t reg_base);
void msc_ram_polling(uint8_t busid);

#endif


 

