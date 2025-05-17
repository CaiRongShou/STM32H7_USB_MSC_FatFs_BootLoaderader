/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */

/* Definitions of physical drive number for each drive */
#define SD_CARD 0
#define SPI_FLASH 1
#define INTER_FLASH 2
#define FLASH_SECTOR_COUNT 1024 /*SPI_Flash SECTOR number*/
#define SECTOR_SIZE 4096 /*SPI_Flash SECTOR size*/
#define FLASH_BLOCK_SIZE 4096 /*smallest unit of erased sector*/
#define SD_CARD_BLOCK_SIZE 1
#define FMC_WRITE_START_ADDR ((uint32_t)0x08000000U)

#include "string.h"
#include "bsp_flash_qspi.h"


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;
	switch(pdrv) {
		case SD_CARD :
			return 0;
		case SPI_FLASH :
			stat = 0;
			return stat;
		case INTER_FLASH:
			stat = 0;
			return stat;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;
	switch(pdrv) {
		case SD_CARD :
			stat &= ~STA_NOINIT;
			return 0;
		case SPI_FLASH :
			return 0;
		case INTER_FLASH:
			stat = 0;
			return stat;
	}
	return STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	uint32_t *ptrd, *btrd;
	DRESULT res;
	switch(pdrv) {
		case SD_CARD :
		
			return RES_OK;
		case SPI_FLASH :
			SPI_FLASH_BufferRead((uint8_t *)buff,(uint32_t)(FALSH_QSPI_MSC_ADDR + (sector*SECTOR_SIZE)),count * SECTOR_SIZE);
			return RES_OK;
		case INTER_FLASH:

			return res;
	}
	return RES_PARERR;
}


/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	DRESULT res;
	switch(pdrv) {
		case SD_CARD :

		return RES_OK;
		case SPI_FLASH:
			bsp_w25qxx_EraseSector((uint32_t)(FALSH_QSPI_MSC_ADDR + (sector*SECTOR_SIZE)));
			SPI_FLASH_BufferWrite((uint8_t *)buff,(uint32_t)(FALSH_QSPI_MSC_ADDR + (sector*SECTOR_SIZE)),count * SECTOR_SIZE);
			return RES_OK;
		case INTER_FLASH:

			res = RES_OK;
		return res;
	}
	return RES_PARERR;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	switch(pdrv) 
	{
		case SD_CARD :
//			switch(cmd) 
//			{
//				/*return sector number*/
//				case GET_SECTOR_COUNT:
//				*(DWORD *)buff = sd_cardinfo.card_capacity / (sd_cardinfo.card_blocksize);
//				break;
//				/*return each sector size*/
//				case GET_SECTOR_SIZE:
//				*(WORD *)buff = sd_cardinfo.card_blocksize;
//				break;
//				/*Returns the smallest unit of erased sector (unit 1)*/
//				case GET_BLOCK_SIZE:
//				*(DWORD *)buff = SD_CARD_BLOCK_SIZE;
//				break;
//			}
			return RES_OK;
		case SPI_FLASH :
			switch(cmd) 
			{
				/*return sector number*/
				case GET_SECTOR_COUNT:
				*(DWORD *)buff = FLASH_SECTOR_COUNT;
				break;
				/*return each sector size*/
				case GET_SECTOR_SIZE:
				*(WORD *)buff = FLASH_SECTOR_SIZE;
				break;
				/*Returns the smallest unit of erased sector (unit 1)*/
				case GET_BLOCK_SIZE:
				*(DWORD *)buff = FLASH_BLOCK_SIZE;
				break;
			}
			res = RES_OK;
			return res;
		case INTER_FLASH:
//			switch(cmd) 
//			{
//				/*return sector number*/
//				case GET_SECTOR_COUNT:
//				*(DWORD *)buff = 128;
//				break;
//				/*return each sector size*/
//				case GET_SECTOR_SIZE:
//				*(WORD *)buff = 2048;
//				break;
//				/*Returns the smallest unit of erased sector (unit 1)*/
//				case GET_BLOCK_SIZE:
//				*(DWORD *)buff = 1;
//				break;
//			}
			res = RES_OK;
			return res;
	}
	return RES_PARERR;
} 
 
 
 