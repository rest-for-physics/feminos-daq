/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        spiflash_polled.c

 Description: Implementation of SpiFlashPolled read/write.

 Adapted from Intel version to Winbond SPI Flash based on sample code provided
 by Xilinx and Enclustra

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2011: created

*******************************************************************************/
#include "spiflash_polled.h"
#include "xparameters.h"

/*
 * Byte Positions.
 */
#define BYTE1 0 /* Byte 1 position */
#define BYTE2 1 /* Byte 2 position */
#define BYTE3 2 /* Byte 3 position */
#define BYTE4 3 /* Byte 4 position */
#define BYTE5 4 /* Byte 5 position */

/*
 * Global variable
 */
SpiFlashPolled spiflashpolled;

/*****************************************************************************
SpiFlashPolled_ReadUniqueID
******************************************************************************/
unsigned char SpiFlashPolled_ReadUniqueID(SpiFlashPolled* sfp) {
    int i;

    /*
     * Clear the read Buffer.
     */
    for (i = 0; i < (WINBOND_FLASH_PAGE_SIZE + WINBOND_READ_WRITE_EXTRA_BYTES); i++) {
        sfp->ReadBuffer[i] = 0x0;
    }

    /*
     * Prepare the WriteBuffer.
     */
    sfp->WriteBuffer[BYTE1] = WINBOND_CMD_UNIQUE_ID;
    sfp->WriteBuffer[BYTE2] = 0;
    sfp->WriteBuffer[BYTE3] = 0;
    sfp->WriteBuffer[BYTE4] = 0;
    sfp->WriteBuffer[BYTE5] = 0;

    /*
     * Initiate the Transfer.
     */
    if (XSpi_Transfer(&(sfp->x_spi), sfp->WriteBuffer, sfp->ReadBuffer, WINBOND_UNIQUE_ID_BYTES) != XST_SUCCESS) {
        return (ERROR_SpiFlashPolled_ReadUniqueID);
    }

    return (0);
}

/*******************************************************************************
 SpiFlashPolled_ReadPagePrepareNext
*******************************************************************************/
unsigned char SpiFlashPolled_ReadPagePrepareNext(SpiFlashPolled* sfp) {
    int i;

    /*
     * Clear the read Buffer.
     */
    for (i = 0; i < (WINBOND_FLASH_PAGE_SIZE + WINBOND_READ_WRITE_EXTRA_BYTES); i++) {
        sfp->ReadBuffer[i] = 0x0;
    }

    /*
     * Prepare the WriteBuffer.
     */
    sfp->WriteBuffer[BYTE1] = WINBOND_CMD_RANDOM_READ;
    sfp->WriteBuffer[BYTE2] = (unsigned char) (sfp->cur_page >> 16);
    sfp->WriteBuffer[BYTE3] = (unsigned char) (sfp->cur_page >> 8);
    sfp->WriteBuffer[BYTE4] = (unsigned char) (sfp->cur_page);

    /*
     * Initiate the Transfer.
     */
    if (XSpi_Transfer(&(sfp->x_spi), sfp->WriteBuffer, sfp->ReadBuffer, (WINBOND_FLASH_PAGE_SIZE + WINBOND_READ_WRITE_EXTRA_BYTES)) != XST_SUCCESS) {
        return (ERROR_SpiFlashPolled_ReadPagePrepareNext);
    }

    /*
     * Initialize current pointer, available size and point to next page
     */
    sfp->cur_ptr = (unsigned char*) &(sfp->ReadBuffer[4]);
    sfp->byte_avail = WINBOND_FLASH_PAGE_SIZE;
    sfp->cur_page += WINBOND_FLASH_PAGE_SIZE;

    return (0);
}

/*******************************************************************************
 SpiFlashPolled_OpenAtPage
*******************************************************************************/
unsigned char SpiFlashPolled_OpenAtPage(SpiFlashPolled* sfp, unsigned int first_page) {
    sfp->byte_avail = 0;
    sfp->cur_page = first_page;
    sfp->cur_ptr = (unsigned char*) first_page;

    /*
     * Initialize the SPI device controller
     */
    if (XSpi_Initialize(&(sfp->x_spi), XPAR_SPI_0_DEVICE_ID) != XST_SUCCESS) {
        return (ERROR_SpiFlashPolled_OpenAtPage);
    }

    /*
     * Set the SPI device as a master and in manual slave select mode such
     * that the slave select signal does not toggle for every byte of a
     * transfer, this must be done before the slave select is set.
     */
    if (XSpi_SetOptions(&(sfp->x_spi), XSP_MASTER_OPTION | XSP_MANUAL_SSELECT_OPTION) != XST_SUCCESS) {
        return (ERROR_SpiFlashPolled_OpenAtPage);
    }

    /*
     * Start the SPI driver so that interrupts and the device are enabled.
     */
    XSpi_Start(&(sfp->x_spi));

    /*
     * Disable Global interrupts to work in polled mode
     */
    XSpi_IntrGlobalDisable(&(sfp->x_spi));

    /*
     * Select the Winbond Serial Flash device,  so that it can be
     * read and written using the SPI bus.
     */
    if (XSpi_SetSlaveSelect(&(sfp->x_spi), WINBOND_SPI_SELECT) != XST_SUCCESS) {
        return (ERROR_SpiFlashPolled_OpenAtPage);
    }

    return (0);
}
