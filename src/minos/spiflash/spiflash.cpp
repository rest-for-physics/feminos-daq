/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        spiflash.c

 Description: Implementation of SpiFlash read/write.

 Adapted from Intel version to Winbond SPI Flash based on sample code provided
 by Xilinx and Enclustra

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2011: created

  September 2012: removed the interrupt controller from the SpiFlash structure
  because it is common to all possible sources of interrupt. A separate
  structure has been created.

  October 2012: added the option to erase or not the sector of the flash
  before a page is written in function SpiFlash_WritePage()

  December 2012: modified SpiFlash_SectorErase() so that this function can be
  called from SpiFlash_WritePage() or directly

*******************************************************************************/
#include "spiflash.h"
#include "error_codes.h"
#include "interrupt.h"
#include "platform_spec.h"
#include <cstdio>

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
SpiFlash spiflash;

/*
 * The following variables are shared between non-interrupt processing and
 * interrupt processing such that they must be global.
 */
volatile static int SpiTransferInProgress;

/*
 * The following variable tracks any errors that occur during interrupt
 * processing.
 */
int SpiErrorCount;

/*****************************************************************************/
/**
 *
 * This function is the handler which performs processing for the SPI driver.
 * It is called from an interrupt context such that the amount of processing
 * performed should be minimized. It is called when a transfer of SPI data
 * completes or an error occurs.
 *
 * This handler provides an example of how to handle SPI interrupts and
 * is application specific.
 *
 * @param	CallBackRef is the upper layer callback reference passed back
 *		when the callback function is invoked.
 * @param	StatusEvent is the event that just occurred.
 * @param	ByteCount is the number of bytes transferred up until the event
 *		occurred.
 *
 * @return	None.
 *
 * @note		None.
 *
 ******************************************************************************/
void SpiHandler(void* CallBackRef, u32 StatusEvent, unsigned int ByteCount) {
    /*
     * Indicate the transfer on the SPI bus is no longer in progress
     * regardless of the status event.
     */
    SpiTransferInProgress = FALSE;

    /*
     * If the event was not transfer done, then track it as an error.
     */
    if (StatusEvent != XST_SPI_TRANSFER_DONE) {
        SpiErrorCount++;
    }
    // printf("SPI Interrupt!\n");
}

/*****************************************************************************/
/**
 *
 * This function enables writes to the Serial Flash memory.
 *
 * @param	SpiPtr is a pointer to the instance of the Spi device.
 *
 * @return	XST_SUCCESS if successful else XST_FAILURE.
 *
 * @note		None.
 *
 ******************************************************************************/
int SpiFlash_WriteEnable(SpiFlash* sf) {
    int Status;

    /*
     * Prepare the WriteBuffer.
     */
    sf->WriteBuffer[BYTE1] = WINBOND_CMD_WRITE_ENABLE;

    /*
     * Initiate the Transfer.
     */
    SpiTransferInProgress = TRUE;
    Status = XSpi_Transfer(&(sf->x_spi), sf->WriteBuffer, NULL, WINBOND_WRITE_ENABLE_BYTES);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /*
     * Wait till the Transfer is complete and check if there are any errors
     * in the transaction..
     */
    while (SpiTransferInProgress);
    if (SpiErrorCount != 0) {
        SpiErrorCount = 0;
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

/*****************************************************************************/
/**
 *
 * This function reads the Status register of the Serial Flash.
 *
 * @param	SpiPtr is a pointer to the instance of the Spi device.
 *
 * @return	XST_SUCCESS if successful else XST_FAILURE.
 *
 * @note		The status register content is stored at the second byte pointed
 *		by the ReadBuffer.
 *
 ******************************************************************************/
int SpiFlash_GetStatus(SpiFlash* sf) {
    int Status;

    /*
     * Prepare the Write Buffer.
     */
    sf->WriteBuffer[BYTE1] = WINBOND_CMD_READ_STATUSREG1;

    /*
     * Initiate the Transfer.
     */
    SpiTransferInProgress = TRUE;
    Status = XSpi_Transfer(&(sf->x_spi), sf->WriteBuffer, sf->ReadBuffer, WINBOND_STATUS_READ_BYTES);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /*
     * Wait till the Transfer is complete and check if there are any errors
     * in the transaction.
     */
    while (SpiTransferInProgress);
    if (SpiErrorCount != 0) {
        SpiErrorCount = 0;
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

/*****************************************************************************/
/**
 *
 * This function waits till the Serial Flash is ready to accept next
 * command.
 *
 * @param	None
 *
 * @return	XST_SUCCESS if successful else XST_FAILURE.
 *
 * @note		This function reads the status register of the Buffer and waits
 *.		till the WIP bit of the status register becomes 0.
 *
 ******************************************************************************/
int SpiFlash_WaitNotBusy(SpiFlash* sf) {
    int Status;
    u8 StatusReg;

    while (1) {
        /*
         * Get the Status Register.
         */
        Status = SpiFlash_GetStatus(sf);
        if (Status != XST_SUCCESS) {
            return XST_FAILURE;
        }

        /*
         * Check if the flash is ready to accept the next command.
         * If so break.
         */
        StatusReg = sf->ReadBuffer[1];
        if ((StatusReg & WINBOND_FLASH_SR_IS_READY_MASK) == 0) {
            break;
        }
    }

    return XST_SUCCESS;
}

/*****************************************************************************/
/**
 *
 * This function erases the contents of the specified Sector in the Intel Serial
 * Flash.
 *
 * @param	SpiPtr is a pointer to the instance of the Spi device.
 * @param	Addr is the address within a sector of the Buffer, which is to
 *		be erased.
 *
 * @return	XST_SUCCESS if successful else XST_FAILURE.
 *
 * @note		The erased bytes will read as 0xFF.
 *
 ******************************************************************************/
int SpiFlash_SectorErase(SpiFlash* sf, unsigned int sect) {
    int err;

    /*
     * Select the Winbond Serial Flash device,  so that it can be
     * read and written using the SPI bus.
     */
    if ((err = XSpi_SetSlaveSelect(&(sf->x_spi), WINBOND_SPI_SELECT)) != XST_SUCCESS) {
        return (-err);
    }
    // printf("XSpi_SetSlaveSelect done.\n");

    /*
     * Perform the Write Enable operation.
     */
    if ((err = SpiFlash_WriteEnable(sf)) != XST_SUCCESS) {
        return (-err);
    }
    // printf(" SpiFlash_WriteEnable done.\n");

    /*
     * Wait till the Flash is not Busy.
     */
    if ((err = SpiFlash_WaitNotBusy(sf)) != XST_SUCCESS) {
        return (-err);
    }
    // printf(" SpiFlash_WaitNotBusy done.\n");

    /*
     * Prepare the WriteBuffer.
     */
    sf->WriteBuffer[BYTE1] = WINBOND_CMD_4K_SECTOR_ERASE;
    sf->WriteBuffer[BYTE2] = (u8) (sect >> 16);
    sf->WriteBuffer[BYTE3] = (u8) (sect >> 8);
    sf->WriteBuffer[BYTE4] = (u8) (sect);

    /*
     * Initiate the Transfer.
     */
    SpiTransferInProgress = TRUE;
    err = XSpi_Transfer(&(sf->x_spi), sf->WriteBuffer, NULL, WINBOND_SECTOR_ERASE_BYTES);
    if (err != XST_SUCCESS) {
        return (-err);
    }

    /*
     * Wait till the Transfer is complete and check if there are any errors
     * in the transaction.
     */
    while (SpiTransferInProgress);
    if (SpiErrorCount != 0) {
        SpiErrorCount = 0;
        return (-XST_FAILURE);
    }

    /*
     * Wait till the Flash is not Busy.
     */
    if ((err = SpiFlash_WaitNotBusy(sf)) != XST_SUCCESS) {
        return (-err);
    }
    // printf(" SpiFlash_WaitNotBusy done.\n");

    /*
     * De-Select the Winbond Serial Flash device
     */
    if ((err = XSpi_SetSlaveSelect(&(sf->x_spi), 0)) != XST_SUCCESS) {
        return (-err);
    }

    return XST_SUCCESS;
}

/*****************************************************************************
SpiFlash_ReadUniqueID
******************************************************************************/
int SpiFlash_ReadUniqueID(SpiFlash* sf, unsigned char** dout) {
    int Status;
    int i;

    /*
     * Clear the read Buffer.
     */
    for (i = 0; i < (WINBOND_FLASH_PAGE_SIZE + WINBOND_READ_WRITE_EXTRA_BYTES); i++) {
        sf->ReadBuffer[i] = 0x0;
    }

    /*
     * Select the Winbond Serial Flash device,  so that it can be
     * read and written using the SPI bus.
     */
    if ((Status = XSpi_SetSlaveSelect(&(sf->x_spi), WINBOND_SPI_SELECT)) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    // printf("XSpi_SetSlaveSelect done.\n");

    /*
     * Prepare the WriteBuffer.
     */
    sf->WriteBuffer[BYTE1] = WINBOND_CMD_UNIQUE_ID;
    sf->WriteBuffer[BYTE2] = 0;
    sf->WriteBuffer[BYTE3] = 0;
    sf->WriteBuffer[BYTE4] = 0;
    sf->WriteBuffer[BYTE5] = 0;

    /*
     * Initiate the Transfer.
     */
    SpiTransferInProgress = TRUE;
    Status = XSpi_Transfer(&(sf->x_spi), sf->WriteBuffer, sf->ReadBuffer, WINBOND_UNIQUE_ID_BYTES);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /*
     * Wait till the Transfer is complete and check if there are any errors
     * in the transaction.
     */
    while (SpiTransferInProgress);
    if (SpiErrorCount != 0) {
        SpiErrorCount = 0;
        return XST_FAILURE;
    }

    /*
    printf("SpiFlash_ReadUniqueID: ");
    for(i=0; i<8; i++)
    {
        printf("0x%x ", sf->ReadBuffer[i+5]);
    }
    printf("\n");
    */

    *dout = &sf->ReadBuffer[5];

    /*
     * De-Select the Winbond Serial Flash device
     */
    if ((Status = XSpi_SetSlaveSelect(&(sf->x_spi), 0)) != XST_SUCCESS) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

/*******************************************************************************
 SpiFlash_ReadPage
*******************************************************************************/
int SpiFlash_ReadPage(SpiFlash* sf, unsigned int page, unsigned int size, unsigned char** dout) {
    int i;
    int err;

    /*
     * Clear the read Buffer.
     */
    for (i = 0; i < (WINBOND_FLASH_PAGE_SIZE + WINBOND_READ_WRITE_EXTRA_BYTES); i++) {
        sf->ReadBuffer[i] = 0x0;
    }

    /*
     * Select the Winbond Serial Flash device,  so that it can be
     * read and written using the SPI bus.
     */
    if ((err = XSpi_SetSlaveSelect(&(sf->x_spi), WINBOND_SPI_SELECT)) != XST_SUCCESS) {
        return (-err);
    }
    // printf("XSpi_SetSlaveSelect done.\n");

    /*
     * Prepare the WriteBuffer.
     */
    sf->WriteBuffer[BYTE1] = WINBOND_CMD_RANDOM_READ;
    sf->WriteBuffer[BYTE2] = (unsigned char) (page >> 16);
    sf->WriteBuffer[BYTE3] = (unsigned char) (page >> 8);
    sf->WriteBuffer[BYTE4] = (unsigned char) (page);

    /*
     * Initiate the Transfer.
     */
    SpiTransferInProgress = TRUE;
    if ((err = XSpi_Transfer(&(sf->x_spi), sf->WriteBuffer, sf->ReadBuffer, (size + WINBOND_READ_WRITE_EXTRA_BYTES))) != XST_SUCCESS) {
        return (-err);
    }

    /*
     * Wait till the Transfer is complete and check if there are any errors
     * in the transaction..
     */
    while (SpiTransferInProgress);
    if (SpiErrorCount != 0) {
        SpiErrorCount = 0;
        return (-XST_FAILURE);
    }

    /*
    printf("SpiIntelFlashRead: Page 0x%08x:\n", page);
    for(i=0; i<WINBOND_FLASH_PAGE_SIZE; i++)
    {
        printf("0x%x ", sf->ReadBuffer[i+WINBOND_READ_WRITE_EXTRA_BYTES]);
    }
    printf("\n");
    */

    *dout = &sf->ReadBuffer[4];

    /*
     * De-Select the Winbond Serial Flash device
     */
    if ((err = XSpi_SetSlaveSelect(&(sf->x_spi), 0)) != XST_SUCCESS) {
        return (-err);
    }

    return (0);
}

/*******************************************************************************
 SpiFlash_WritePage
*******************************************************************************/
int SpiFlash_WritePage(SpiFlash* sf, unsigned int erase, unsigned int page, unsigned int size, unsigned char* data) {
    int i;
    int err;
    unsigned int sector;

    /*
     * Check that page is aligned on page size boundary
     */
    if (page & (WINBOND_FLASH_PAGE_SIZE - 1)) {
        return (ERR_ILLEGAL_PARAMETER);
    }
    // printf("SpiFlash_WritePage started.\n");

    if (erase) {
        /*
         * Perform the Sector Erase operation.
         */
        sector = page & 0xFFFFF000;
        if ((err = SpiFlash_SectorErase(sf, sector)) != XST_SUCCESS) {
            return (-err);
        }
        // printf(" SpiFlash_SectorErase done.\n");
    }

    /*
     * Select the Winbond Serial Flash device,  so that it can be
     * read and written using the SPI bus.
     */
    if ((err = XSpi_SetSlaveSelect(&(sf->x_spi), WINBOND_SPI_SELECT)) != XST_SUCCESS) {
        return (-err);
    }
    // printf("XSpi_SetSlaveSelect done.\n");

    /*
     * Perform the Write Enable operation.
     */
    if ((err = SpiFlash_WriteEnable(sf)) != XST_SUCCESS) {
        return (-err);
    }
    // printf(" SpiFlash_WriteEnable done.\n");

    /*
     * Wait till the Flash is not Busy.
     */
    if ((err = SpiFlash_WaitNotBusy(sf)) != XST_SUCCESS) {
        return (-err);
    }
    // printf(" SpiFlash_WaitNotBusy done.\n");

    /*
     * Prepare the WriteBuffer.
     */
    sf->WriteBuffer[BYTE1] = WINBOND_CMD_PAGE_PROGRAM;
    sf->WriteBuffer[BYTE2] = (u8) (page >> 16);
    sf->WriteBuffer[BYTE3] = (u8) (page >> 8);
    sf->WriteBuffer[BYTE4] = (u8) (page);

    /*
     * Copy the data from user supplied buffer to SPI Flash Write Buffer
     */
    for (i = 0; i < size; i++) {
        sf->WriteBuffer[i + WINBOND_READ_WRITE_EXTRA_BYTES] = *(data + i);
    }

    /*
     * Initiate the Transfer.
     */
    SpiTransferInProgress = TRUE;
    if ((err = XSpi_Transfer(&(sf->x_spi), sf->WriteBuffer, NULL, (size + WINBOND_READ_WRITE_EXTRA_BYTES))) != XST_SUCCESS) {
        return (-err);
    }
    // printf(" XSpi_Transfer posted.\n");

    /*
     * Wait till the Transfer is complete and check if there are any errors
     * in the transaction..
     */
    while (SpiTransferInProgress);
    if (SpiErrorCount != 0) {
        SpiErrorCount = 0;
        return (-XST_FAILURE);
    }

    /*
     * Wait till the Flash is not Busy.
     */
    if ((err = SpiFlash_WaitNotBusy(sf)) != XST_SUCCESS) {
        return (-err);
    }
    // printf(" SpiFlash_WaitNotBusy done.\n");

    /*
     * De-Select the Winbond Serial Flash device
     */
    if ((err = XSpi_SetSlaveSelect(&(sf->x_spi), 0)) != XST_SUCCESS) {
        return (-err);
    }

    // printf("SpiFlash_WritePage done.\n");

    return (0);
}

/*******************************************************************************
 SpiFlash_Open
*******************************************************************************/
int SpiFlash_Open(SpiFlash* sf, XIntc* x_intc) {
    int err = 0;

    // Initialize the SPI device controller
    if ((err = XSpi_Initialize(&(sf->x_spi), XPAR_SPI_0_DEVICE_ID)) != XST_SUCCESS) {
        return (-err);
    }

    /*
     * Connect a device driver handler that will be called when an interrupt
     * for the device occurs, the device driver handler performs the
     * specific interrupt processing for the device
     */
    if ((err = XIntc_Connect(x_intc, XPAR_INTC_0_SPI_0_VEC_ID, (XInterruptHandler) XSpi_InterruptHandler, (void*) &(sf->x_spi))) != XST_SUCCESS) {
        return (-err);
    }

    /*
     * Enable the interrupt for the SPI.
     */
    XIntc_Enable(x_intc, XPAR_INTC_0_SPI_0_VEC_ID);

    /*
     * Setup the handler for the SPI that will be called from the interrupt
     * context when an SPI status occurs, specify a pointer to the SPI
     * driver instance as the callback reference so the handler is able to
     * access the instance data.
     */
    XSpi_SetStatusHandler(&(sf->x_spi), &(sf->x_spi), (XSpi_StatusHandler) SpiHandler);

    /*
     * Set the SPI device as a master and in manual slave select mode such
     * that the slave select signal does not toggle for every byte of a
     * transfer, this must be done before the slave select is set.
     */
    if ((err = XSpi_SetOptions(&(sf->x_spi), XSP_MASTER_OPTION | XSP_MANUAL_SSELECT_OPTION)) != XST_SUCCESS) {
        return (-err);
    }

    /*
     * Start the SPI driver so that interrupts and the device are enabled.
     */
    XSpi_Start(&(sf->x_spi));

    return (0);
}
