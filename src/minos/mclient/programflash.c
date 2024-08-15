/*******************************************************************************

 File:        programflash.c

 Description: Functions to reprogram Feminos or TCM SPI Flash memory with
  a new bitstream


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
   October 2011 : created

*******************************************************************************/
#include "programflash.h"
#include "evbuilder.h"
#include "femarray.h"
#include "hexreader.h"

extern int verbose;

#include <stdio.h>

#define MAX_LINE_SIZE 120
#define FLASH_PAGE_SIZE 256
#define FLASH_SECTOR_SIZE 4096

static char hex2ascii[17] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/*******************************************************************************
 CmdFetcher_SendFlashPage
*******************************************************************************/
int CmdFetcher_SendFlashPage(CmdFetcher* cf, int ns, unsigned int adr, int sz, unsigned char* data) {
    int i;
    char command[1024];
    char* c;
    unsigned char* d;
    int err;
    unsigned int fem_beg, fem_end, fem_pat;
    FemArray* fa;

    fa = (FemArray*) cf->fa;

    fem_beg = cf->cur_single_fem;
    fem_end = cf->cur_single_fem;
    fem_pat = 1 << cf->cur_single_fem;

    // Make the command header
    if (ns) {
        sprintf(&command[0], "flash erase_write_verify 0x%08x 0x%03x ", adr, sz);
    } else {
        sprintf(&command[0], "flash write_verify 0x%08x 0x%03x ", adr, sz);
    }
    c = &command[0] + strlen(command);

    // Put data in ASCII in the command
    d = data;
    for (i = 0; i < sz; i++) {
        *c = hex2ascii[(*d >> 4) & 0xF];
        c++;
        *c = hex2ascii[(*d) & 0xF];
        c++;
        d++;
    }
    *c = '\n';
    c++;
    *c = '\0';

    // printf("%s", command);

    // Pass command to FemArray that will send it
    if ((err = FemArray_SendCommand(fa, fem_beg, fem_end, fem_pat, &command[0])) < 0) {
        return (err);
    }

    // Wait completion of the command to be signalled by network receive thread
    if ((err = Semaphore_Wait_Timeout(cf->sem_cur_cmd_done, 4000000)) < 0) {
        if (err == -2) {
            printf("CmdFetcher_SendFlashPage: Semaphore_Wait_Timeout: timeout detected.\n", err);
        } else {
            printf("CmdFetcher_SendFlashPage: Semaphore_Wait_Timeout failed %d\n", err);
        }
        return (err);
    }
    cf->cmd_index++;

    return (err);
}

/*******************************************************************************
 CmdFetcher_ProgramFlash
*******************************************************************************/
int CmdFetcher_ProgramFlash(CmdFetcher* cf) {
    int i;
    int err, err2;
    HexContext hc;
    FemArray* fa;
    unsigned int fem_beg;
    int done;
    unsigned int target_address;
    unsigned int page_start_adr;
    int data_byte_cnt;
    unsigned char data_bytes[16];
    unsigned char page_image[FLASH_PAGE_SIZE + 16];
    int cur_page_ptr;
    unsigned int cur_sector;
    unsigned int tmp_sector;
    int is_new_sector;
    int page_load;
    int page_count;
    int total_byte_cnt;
    int err_cnt_beg;
    int err_cnt_end;
    int verbose_saved;

    err = 0;

    // save verbose
    verbose_saved = verbose;
    // be silent
    verbose = 0;

    fa = (FemArray*) cf->fa;

    fem_beg = cf->cur_single_fem;
    // Get the initial number of command errors for that fem
    err_cnt_beg = fa->fp[fem_beg].cmd_failed;

    printf("Programming Card %d with firmware \"%s\"\n", fem_beg, &cf->cmd_file[0]);

    // Init
    done = 0;
    page_count = 0;
    page_load = 0;
    total_byte_cnt = 0;
    target_address = 0;
    page_start_adr = 0;
    cur_sector = (FLASH_SECTOR_SIZE - 1); // non-exising sector
    tmp_sector = 0;
    is_new_sector = 0;
    cur_page_ptr = 0;

    // Open HexReader
    if ((err = HexReader_Open(&hc, &cf->cmd_file[0])) < 0) {
        printf("CmdFetcher_ProgramFlash: HexReader_Open: %s\n", hc.rep);
        goto end;
    }

    // Main loop
    while (!done) {
        // Get Data from HEX file
        data_byte_cnt = HexReader_ReadBytes(&hc, 16, &target_address, &data_bytes[0]);

        if (data_byte_cnt < 0) {
            printf("HexReader_ReadBytes:%s\n", hc.rep);
            goto end;
        } else if (hc.is_eof) {
            done = 1;
        }

        // printf("HexReader_ReadBytes: 0x%x  %d bytes\n", target_address, data_byte_cnt);

        if (((target_address > (page_start_adr + FLASH_PAGE_SIZE)) || (page_load == 256) || (done == 1)) && (page_load != 0)) {
            // printf("start address 0x%08x %d bytes\n", page_start_adr, page_load);

            // Send this page
            if ((err = CmdFetcher_SendFlashPage(cf, is_new_sector, page_start_adr, page_load, &page_image[0])) < 0) {
                printf("CmdFetcher_SendFlashPage: failed with error status %d.\n", err);
                goto end;
            }

            page_count++;
            page_load = 0;

            if ((page_count % 128) == 0) {
                printf(".");
                fflush(stdout);
            }
        }

        // Are we starting to fill a new page?
        if ((page_load == 0) && (data_byte_cnt != 0)) {
            // Clear the page image
            for (cur_page_ptr = 0; cur_page_ptr < FLASH_PAGE_SIZE; cur_page_ptr++) {
                page_image[cur_page_ptr] = 0x00;
            }
            cur_page_ptr = 0;
            page_start_adr = target_address;

            // Get the current sector
            tmp_sector = target_address & (~(FLASH_SECTOR_SIZE - 1));

            // Are we in a new sector?
            if (tmp_sector != cur_sector) {
                cur_sector = tmp_sector;
                is_new_sector = 1;
            } else {
                is_new_sector = 0;
            }
        }

        // Copy the data read from file in the current page
        for (i = 0; i < data_byte_cnt; i++) {
            page_image[cur_page_ptr] = data_bytes[i];
            cur_page_ptr++;
        }
        page_load += data_byte_cnt;
        total_byte_cnt += data_byte_cnt;

        // Check that all data could fit in the current page
        if (page_load > 256) {
            printf("CmdFetcher_ProgramFlash: page %d has an incorrect size of %d bytes\n", page_count, page_load);
            goto end;
        }
    }
    printf("\n");

end:

    // restore verbose
    verbose = verbose_saved;

    // Do not echo this command to console
    cf->no_echo_this = 1;

    // Get the final number of command errors for that fem
    err_cnt_end = fa->fp[fem_beg].cmd_failed;
    if ((err_cnt_end - err_cnt_beg) != 0) {
        err = -1;
    }

    printf("File lines     : %d\n", hc.line_count);
    printf("Byte count     : %d\n", total_byte_cnt);
    printf("Flash pages    : %d\n", page_count);
    printf("Command failed : %d\n", err_cnt_end - err_cnt_beg);

    err2 = HexReader_Close(&hc);

    if (err < 0) {
        return (err);
    } else {
        return (err2);
    }
}
