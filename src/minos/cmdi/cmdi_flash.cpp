/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        cmdi_flash.c

 Description: Implementation of command interpreter function to read/write
  SPI Flash

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr

 History:

  October 2012: created Flash commands to read / write pages in the external
  SPI Flash to allow fimware/software upgrade via Ethernet instead of JTAG

*******************************************************************************/

#include "cmdi_flash.h"
#include "error_codes.h"
#include "spiflash.h"

#include <cstdio>
#include <string.h>

static char hex2ascii[17] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/*******************************************************************************
 Cmdi_FlashCommands
*******************************************************************************/
int Cmdi_FlashCommands(CmdiContext* c) {
    unsigned int i;
    unsigned int param[10];
    unsigned int page, size;
    char* rep;
    unsigned char* dout;
    unsigned char din[WINBOND_FLASH_PAGE_SIZE];
    char* ch;
    unsigned char* d;
    int err;
    unsigned int is_erase_write;
    unsigned int is_write;
    unsigned int is_verify;
    unsigned char wr_buf[1024];
    char tmp[32];
    int mismatch;

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME

    is_erase_write = 0;
    is_write = 0;
    is_verify = 0;
    mismatch = 0;

    // Command to read a flash region
    if (strncmp(c->cmd + 6, "read", 4) == 0) {
        if (sscanf(c->cmd + 6, "read %i %i", &param[0], &param[1]) == 2) {
            // Limit to 32 bytes what is readout
            if (param[1] > 32) {
                param[1] = 32;
            }

            page = param[0] & ~(WINBOND_FLASH_PAGE_SIZE - 1);
            size = param[0] + param[1] - page;

            // Read Flash page
            if ((err = SpiFlash_ReadPage(&spiflash, page, size, &dout)) < 0) {
                sprintf(rep, "%d %s(%02d): SpiFlash_ReadPage failed\n", err, &(c->fem->name[0]), c->fem->card_id);
            } else {
                sprintf(rep, "0 %s(%02d) Flash 0x%08x ", &(c->fem->name[0]), c->fem->card_id, param[0]);
                ch = rep + strlen(rep);
                d = dout + (param[0] & (WINBOND_FLASH_PAGE_SIZE - 1));

                for (i = 0; i < param[1]; i++) {
                    *ch = hex2ascii[(*d >> 4) & 0xF];
                    ch++;
                    *ch = hex2ascii[*d & 0xF];
                    ch++;
                    d++;
                }
                *ch = '\n';
                ch++;
                *ch = '\0';
            }
        } else {
            err = -1;
            sprintf(rep, "%d %s(%02d): missing argument after flash read\n", err, &(c->fem->name[0]), c->fem->card_id);
        }
    }

    // Command to write a flash region w/wo sector erase and w/wo verify
    else if (sscanf(c->cmd + 6, "%s %i %i %s", &tmp[0], &param[0], &param[1], &wr_buf[0]) == 4) {
        if (strncmp(c->cmd + 6, "erase_write_verify", 18) == 0) {
            is_erase_write = 1;
            is_write = 0;
            is_verify = 1;
            err = 0;
        } else if (strncmp(c->cmd + 6, "erase_write", 11) == 0) {
            is_erase_write = 1;
            is_write = 0;
            is_verify = 1;
            err = 0;
        } else if (strncmp(c->cmd + 6, "write_verify", 12) == 0) {
            is_erase_write = 0;
            is_write = 1;
            is_verify = 1;
            err = 0;
        } else if (strncmp(c->cmd + 6, "write", 5) == 0) {
            is_erase_write = 0;
            is_write = 1;
            is_verify = 0;
            err = 0;
        } else {
            err = -1;
            sprintf(rep, "%d %s(%02d): incorrect arguments\n", err, &(c->fem->name[0]), c->fem->card_id);
        }
    }

    // Syntax error or unsupported command
    else {
        err = -1;
        sprintf(rep, "%d %s(%02d): flash [read|write|erase_write] 0xAdr 0xSize data...\n", err, &(c->fem->name[0]), c->fem->card_id);
    }

    // Now do the erase and write
    if (is_erase_write || is_write) {
        // Check that the base address is aligned on a page boundary
        if (param[0] & (WINBOND_FLASH_PAGE_SIZE - 1)) {
            err = -1;
            sprintf(rep, "%d %s(%02d): address 0x%x is not aligned on page boundary (%d bytes)\n", err, &(c->fem->name[0]), c->fem->card_id, param[0], (WINBOND_FLASH_PAGE_SIZE - 1));
        }
        // Check that the amount of data to write is at most one page
        else if (param[1] > WINBOND_FLASH_PAGE_SIZE) {
            err = -1;
            sprintf(rep, "%d %s(%02d): data size (%d bytes) is more than page size (%d bytes).\n", err, &(c->fem->name[0]), c->fem->card_id, param[1], WINBOND_FLASH_PAGE_SIZE);
        }
        // Check the size of the data buffer
        else if ((2 * param[1]) != strlen((char*) &wr_buf[0])) {
            err = -1;
            sprintf(rep, "%d %s(%02d): data size mismatch char_expected:%d char_supplied:%d\n", err, &(c->fem->name[0]), c->fem->card_id, 2 * param[1], (int) strlen((char*) &wr_buf[0]));
        } else {
            // Prepare data buffer
            sprintf(tmp, "0x00");
            for (i = 0; i < param[1]; i++) {
                tmp[2] = wr_buf[2 * i];
                tmp[3] = wr_buf[(2 * i) + 1];
                sscanf(tmp, "0x%x", &param[2]);
                din[i] = (unsigned char) (param[2] & 0xFF);
            }

            // Write the page
            if ((err = SpiFlash_WritePage(&spiflash, is_erase_write, param[0], param[1], &din[0])) < 0) {
                sprintf(rep, "%d %s(%02d): SpiFlash_WritePage failed.\n", err, &(c->fem->name[0]), c->fem->card_id);
            } else {
                err = 0;
                sprintf(rep, "%d %s(%02d): Flash written\n", err, &(c->fem->name[0]), c->fem->card_id);
            }
        }
    }

    // Now do the verify
    if (is_verify) {
        page = param[0] & ~(WINBOND_FLASH_PAGE_SIZE - 1);
        size = param[0] + param[1] - page;

        // Read Flash page
        if ((err = SpiFlash_ReadPage(&spiflash, page, size, &dout)) < 0) {
            sprintf(rep, "%d %s(%02d): SpiFlash_ReadPage failed\n", err, &(c->fem->name[0]), c->fem->card_id);
        } else {
            // Perform byte to byte comparison between write buffer and read buffer
            mismatch = 0;
            for (i = 0; i < param[1]; i++) {
                if (*(dout + i) != din[i]) {
                    mismatch++;
                }
            }

            // Make final result string
            if (mismatch == 0) {
                err = 0;
                sprintf(rep, "%d %s(%02d): Flash written and verified\n", err, &(c->fem->name[0]), c->fem->card_id);
            } else {
                err = -1;
                sprintf(rep, "%d %s(%02d): Flash content read-back does not match data written on %d bytes\n", err, &(c->fem->name[0]), c->fem->card_id, mismatch);
            }
        }
    }

    c->do_reply = 1;
    return (err);
}
