/////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2009 Xilinx, Inc. All Rights Reserved.
//
// You may copy and modify these files for your own internal use solely with
// Xilinx programmable logic devices and  Xilinx EDK system or create IP
// modules solely for Xilinx programmable logic devices and Xilinx EDK system.
// No rights are granted to distribute any files unless they are distributed in
// Xilinx programmable logic devices.
//
/////////////////////////////////////////////////////////////////////////////////

/*
 *      Simple SREC Bootloader
 *      This simple bootloader is provided with Xilinx EDK for you to easily re-use in your
 *      own software project. It is capable of booting an SREC format image file
 *      (Mototorola S-record format), given the location of the image in memory.
 *      In particular, this bootloader is designed for images stored in non-volatile flash
 *      memory that is addressable from the processor.
 *
 *      Please modify the define "FLASH_IMAGE_BASEADDR" in the blconfig.h header file
 *      to point to the memory location from which the bootloader has to pick up the
 *      flash image from.
 *
 *      You can include these sources in your software application project in XPS and
 *      build the project for the processor for which you want the bootload to happen.
 *      You can also subsequently modify these sources to adapt the bootloader for any
 *      specific scenario that you might require it for.
 *
 */
/*
 * Adapted to load configuration from SPI Flash
 * The SPI Flash controller is used in polled mode and a minimum set of function is provided
 *
 * Author: D. Calvet
 *
 * Date and history:
 *  October 2011: adapted from parallel Flash bootloader
 */

#include "blconfig.h"
#include "errors.h"
#include "portab.h"
#include "spiflash_polled.h"
#include "srec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defines */
#define CR 13

/* Comment the following line, if you want a smaller and faster bootloader which will be silent */
#define VERBOSE

/* Declarations */
static void display_progress(uint32_t lines);
static uint8_t load_exec();
static uint8_t flash_get_srec_line(uint8_t* buf);
extern void init_stdout();

#define DISPLAY_SREC_PROGRESS_PERIOD 100

extern int srec_line;

#ifdef __cplusplus
extern "C" {
#endif

extern void outbyte(char c);

#ifdef __cplusplus
}
#endif

/* Data structures */
static srec_info_t srinfo;
static uint8_t sr_buf[SREC_MAX_BYTES];
static uint8_t sr_data_buf[SREC_DATA_MAX_BYTES];

#ifdef VERBOSE

static int8_t* errors[] = {
        "",
        "Error while copying executable image into RAM",
        "Error while reading an SREC line from flash",
        "SREC line is corrupted",
        "SREC has invalid checksum.",
        "SpiFlashPolled_ReadUniqueID failed"
        "SpiFlashPolled_OpenAtPage failed"
        "SpiFlashPolled_ReadPagePrepareNext failed"};

static char hex2ascii[16] = "0123456789ABCDEF";

#endif

/* We don't use interrupts/exceptions.
   Dummy definitions to reduce code size on MicroBlaze */
#ifdef __MICROBLAZE__
void _interrupt_handler() {}
void _exception_handler() {}
void _hw_exception_handler() {}
#endif

/*******************************************************************************
 main
*******************************************************************************/
int main() {
    uint8_t ret;
    int i;
    unsigned char hexdigit;

    /* Initialize stdout */
    init_stdout();

#ifdef VERBOSE

    /* Open the SPI Flash */
    if ((ret = SpiFlashPolled_OpenAtPage(&spiflashpolled, FLASH_IMAGE_BASEADDR)) != 0) {
        goto fatal;
    }

    /* Read the unique ID of the SPI Flash */
    if ((ret = SpiFlashPolled_ReadUniqueID(&spiflashpolled)) != 0) {
        goto fatal;
    }
    print("SpiFlash_ReadUniqueID: hex' ");
    for (i = 0; i < 8; i++) {
        hexdigit = ((spiflashpolled.ReadBuffer[5 + i]) >> 4) & 0xF;
        outbyte(hex2ascii[hexdigit]);
        hexdigit = ((spiflashpolled.ReadBuffer[5 + i]) >> 0) & 0xF;
        outbyte(hex2ascii[hexdigit]);
        print(".");
    }
    print("\r\n");

    print("Loading program from Flash base address 0x");
    putnum((uint32_t) FLASH_IMAGE_BASEADDR);
    print("\r\n");

#endif

    ret = load_exec();

    /* If we reach here, we are in error */

fatal:

#ifdef VERBOSE
    if ((ret == SREC_PARSE_ERROR) || (ret == SREC_CKSUM_ERROR)) {
        print("ERROR in SREC line: ");
        putnum(srec_line);
        print(errors[ret]);
    } else {
        print("ERROR: ");
        print(errors[ret]);
    }
#endif

    return ret;
}

#ifdef VERBOSE
static void display_progress(uint32_t count) {
    /* Send carriage return */
    outbyte(CR);
    print("Processed (0x)");
    putnum(count);
    print(" S-records");
}
#endif

/*
 * Get all S-REC from the Flash until the jump to boot address is found
 */
static uint8_t load_exec() {
    uint8_t ret;
    void (*laddr)();
    int8_t done = 0;

    srinfo.sr_data = sr_data_buf;

    while (!done) {
        /* Get the next S-REC line from the Flash */
        if ((ret = flash_get_srec_line(sr_buf)) != 0) {
            return ret;
        }

        /* Decode the S-REC line we just read */
        if ((ret = decode_srec_line(sr_buf, &srinfo)) != 0) {
            return ret;
        }

#ifdef VERBOSE
        /* Display progress once every xx SREC line */
        if ((srec_line % DISPLAY_SREC_PROGRESS_PERIOD) == 1) {
            display_progress(srec_line);
        }
#endif

        /* Process the decoded SREC-line */
        switch (srinfo.type) {
            case SREC_TYPE_0:
                break;
            case SREC_TYPE_1:
            case SREC_TYPE_2:
            case SREC_TYPE_3:
                memcpy((void*) srinfo.addr, (void*) srinfo.sr_data, srinfo.dlen);
                break;
            case SREC_TYPE_5:
                break;
            case SREC_TYPE_7:
            case SREC_TYPE_8:
            case SREC_TYPE_9:
                laddr = (void (*)()) srinfo.addr;
                done = 1;
                ret = 0;
                break;
        }
    }

#ifdef VERBOSE
    print("\r\nExecuting program starting at address: ");
    putnum((uint32_t) laddr);
    print("\r\n");
#endif

    (*laddr)();

    /* We will be dead at this point */
    return 0;
}

/*
 * Get a S-RECORD from the Flash
 */
static uint8_t flash_get_srec_line(uint8_t* buf) {
    uint8_t c;
    unsigned char err;
    int count = 0;

    /* Extract all the bytes of the current line until end of line is found */
    while (1) {
        /* If no data available from Flash, read the current page and point to the next */
        if (spiflashpolled.byte_avail == 0) {
            if ((err = SpiFlashPolled_ReadPagePrepareNext(&spiflashpolled)) != 0) {
                return (err);
            }
        }

        /* Get the next character from Flash buffer */
        c = *(spiflashpolled.cur_ptr);
        spiflashpolled.cur_ptr++;
        spiflashpolled.byte_avail--;

        /* Check if we have reached the end of line */
        if (c == 0xD) {
            /* Eat up the 0xA too */
            c = *(spiflashpolled.cur_ptr);
            spiflashpolled.cur_ptr++;
            spiflashpolled.byte_avail--;
            /* Now we have a complete line in the line buffer */
            return (0);
        }

        /* Copy the character to the current line buffer */
        *buf++ = c;
        count++;

        /* Make sure max line size not reached */
        if (count > SREC_MAX_BYTES) {
            return (LD_SREC_LINE_ERROR);
        }
    }
}

#ifdef __PPC__

#include <unistd.h>

/* Save some code and data space on PowerPC
   by defining a minimal exit */
void exit(int ret) {
    _exit(ret);
}
#endif
