/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        spiflash.h

 Description: Definition of SpiFlash API.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2011: created

*******************************************************************************/

#ifndef SPIFLASH_H
#define SPIFLASH_H

#include "w25q128bv.h"
#include "xintc.h"
#include "xspi.h"

/*
 * The following constant defines the slave select signal that is used to
 * to select the Flash device on the SPI bus, this signal is typically
 * connected to the chip select of the device.
 */
#define WINBOND_SPI_SELECT 0x01

typedef struct _SpiFlash {
    unsigned char ReadBuffer[WINBOND_FLASH_PAGE_SIZE + WINBOND_READ_WRITE_EXTRA_BYTES];
    unsigned char WriteBuffer[WINBOND_FLASH_PAGE_SIZE + WINBOND_READ_WRITE_EXTRA_BYTES];
    XSpi x_spi;
} SpiFlash;

/*
 * Global variable defined in spiflash.c
 */
extern SpiFlash spiflash;

/*
 * API function prototypes
 */
int SpiFlash_Open(SpiFlash* sf, XIntc* x_intc);
int SpiFlash_SectorErase(SpiFlash* sf, unsigned int sect);
int SpiFlash_WritePage(SpiFlash* sf, unsigned int erase, unsigned int page, unsigned int size, unsigned char* data);
int SpiFlash_ReadPage(SpiFlash* sf, unsigned int page, unsigned int size, unsigned char** dout);
int SpiFlash_ReadUniqueID(SpiFlash* sf, unsigned char** dout);

#endif
