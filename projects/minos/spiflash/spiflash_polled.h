/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        spiflash_polled.h

 Description: Definition of SpiFlash API for inclusion in a bootloader


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  October 2011: created
  
*******************************************************************************/

#ifndef SPIFLASH_POLLED_H
#define SPIFLASH_POLLED_H

#include "xspi.h"
#include "w25q128bv.h"

/*
 * Error definitions (extension of those defined by bootloader)
 */
#define ERROR_SpiFlashPolled_ReadUniqueID        5
#define ERROR_SpiFlashPolled_OpenAtPage          6
#define ERROR_SpiFlashPolled_ReadPagePrepareNext 7


/*
 * The following constant defines the slave select signal that is used to
 * to select the Flash device on the SPI bus, this signal is typically
 * connected to the chip select of the device.
 */
#define WINBOND_SPI_SELECT 0x01

typedef struct _SpiFlashPolled {
	XSpi          x_spi;
	unsigned char ReadBuffer[WINBOND_FLASH_PAGE_SIZE + WINBOND_READ_WRITE_EXTRA_BYTES];
	unsigned char WriteBuffer[WINBOND_FLASH_PAGE_SIZE + WINBOND_READ_WRITE_EXTRA_BYTES];
	unsigned int  cur_page;
	unsigned char *cur_ptr;
	int           byte_avail;
} SpiFlashPolled;

extern SpiFlashPolled spiflashpolled;

unsigned char SpiFlashPolled_OpenAtPage(SpiFlashPolled *sfp, unsigned int first_page);
unsigned char SpiFlashPolled_ReadUniqueID(SpiFlashPolled *sfp);
unsigned char SpiFlashPolled_ReadPagePrepareNext(SpiFlashPolled *sfp);


#endif
