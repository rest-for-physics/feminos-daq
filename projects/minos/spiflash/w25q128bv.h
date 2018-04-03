/*******************************************************************************

 File:        w25q128bv.h

 Description: Definition of commands for Winbond W25Q128BV SPI Flash


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  October 2011: created
  
*******************************************************************************/

#ifndef W25Q128BV_H
#define W25Q128BV_H

/*
 * Definitions of commands
 */
#define	WINBOND_CMD_WRITE_ENABLE     0x06 /* Write Enable command */
#define	WINBOND_CMD_WRITE_DISABLE    0x04 /* Write Disable command */
#define WINBOND_CMD_READ_STATUSREG1  0x05 /* Status read command */
#define WINBOND_CMD_READ_STATUSREG2  0x35 /* Status read command */
#define WINBOND_CMD_WRITE_STATUSREG1 0x01 /* Status write command */
#define WINBOND_CMD_PAGE_PROGRAM     0x02 /* Page Program command */
#define WINBOND_CMD_4K_SECTOR_ERASE  0x20 /* 4 KB Sector Erase command */
#define WINBOND_CMD_32K_BLOCK_ERASE  0x52 /* 32 KB Block Erase command */
#define WINBOND_CMD_64K_BLOCK_ERASE  0xD8 /* 64 KB Block Erase command */
#define WINBOND_CMD_CHIP_ERASE       0xC7 /* Chip Erase Erase command */
#define WINBOND_CMD_RANDOM_READ      0x03 /* Random read command */
#define WINBOND_CMD_DEVICE_ID        0x90 /* Manufacturer Device ID */
#define WINBOND_CMD_UNIQUE_ID        0x4B /* Unique Device ID */

/*
 * This definitions specify the EXTRA bytes for each of the command
 * transactions. This count includes command byte, address bytes and any
 * don't care bytes needed.
 */
#define WINBOND_READ_WRITE_EXTRA_BYTES 4 /* Read/Write extra bytes */
#define	WINBOND_WRITE_ENABLE_BYTES     1 /* Write Enable bytes */
#define WINBOND_SECTOR_ERASE_BYTES     4 /* Sector erase extra bytes */
#define WINBOND_CHIP_ERASE_BYTES       1 /* Chip erase extra bytes */
#define WINBOND_STATUS_READ_BYTES      2 /* Status read bytes count */
#define WINBOND_STATUS_WRITE_BYTES     2 /* Status write bytes count */
#define WINBOND_UNIQUE_ID_BYTES       13 /* Unique ID Byte count */

/*
 * Number of bytes per page in the flash device.
 */
#define WINBOND_FLASH_PAGE_SIZE      256
#define WINBOND_FLASH_SECTOR_SIZE   4096

/*
 * Flash not busy mask in the status register of the flash device.
 */
#define WINBOND_FLASH_SR_IS_READY_MASK 0x01 /* Ready mask */

#endif
