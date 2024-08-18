/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        bufpool.c

 Description: Implementation of Buffer Pool entity.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  June 2009: created

  November 2010: added BufPool_GetBufferFlags()

*******************************************************************************/

#include "bufpool.h"
#include "bufpool_err.h"
#include <cstdio>

/*******************************************************************************
 BufPool_Init
*******************************************************************************/
void BufPool_Init(BufPool* bp) {
    int i;
    int j;

    for (i = 0; i < POOL_NB_OF_BUFFER; i++) {
        for (j = 0; j < POOL_BUFFER_SIZE; j++) {
            bp->buf[i][j] = 0x00;
        }
        bp->busy[i] = BUFFER_FREE;
        // printf("0 BufPool_Init: buf(%d)=0x%x 0x%x\r\n", i, &(bp->buf[i][0]), &(bp->buf[i][POOL_BUFFER_SIZE-1]));
    }
    bp->cur_buf_ix = 0;
    bp->free_cnt = POOL_NB_OF_BUFFER;
    // printf("0 BufPool_Init: bufmin=0x%x  bufmax=0x%x\r\n", (unsigned int) &(bp->buf[0][0]), (unsigned int) &(bp->buf[POOL_NB_OF_BUFFER-1][0]));
}

/*******************************************************************************
 BufPool_GiveBuffer
*******************************************************************************/
int BufPool_GiveBuffer(BufPool* bp, void** bu, unsigned char flags) {
    int i;

    // There should be at least one free buffer in the array
    if (bp->free_cnt) {
        // Scan the array
        for (i = 0; i < POOL_NB_OF_BUFFER; i++) {
            // the current buffer should be the one which is free
            if (bp->busy[bp->cur_buf_ix] == BUFFER_FREE) {
                // if the count is consistent
                if (bp->free_cnt > 1) {
                    *bu = (void*) (&bp->buf[bp->cur_buf_ix][0]);
                    bp->busy[bp->cur_buf_ix] = BUFFER_BUSY | flags;
                    bp->cur_buf_ix = (bp->cur_buf_ix + 1) % POOL_NB_OF_BUFFER;
                    bp->free_cnt--;
                    /*
                    printf("0 BufPool_GiveBuffer: buf=0x%x free_cnt=%d\n", (unsigned int) *bu, bp->free_cnt);
                    */
                    return (0);
                }
                // something went wrong with the free buffer count
                else {
                    *bu = (void*) 0;
                    // printf("BufPool_GiveBuffer: no buffer! free_cnt=%d cur_buf_ix=%d\r\n", bp->free_cnt, bp->cur_buf_ix);
                    return (ERR_BUFPOOL_BUFFER_FREE_COUNT_UNDERRANGE);
                }
            } else
            // Move to the next buffer in the array
            {
                bp->cur_buf_ix = (bp->cur_buf_ix + 1) % POOL_NB_OF_BUFFER;
            }
        }
        // there was presumably at least one free buffer but we could not find it
        *bu = (void*) 0;
        return (ERR_BUFPOOL_FREE_BUFFER_NOT_FOUND);
    } else
    // No free buffer
    {
        *bu = (void*) 0;
        // printf("BufPool_GiveBuffer: no buffer! free_cnt=%d cur_buf_ix=%d\r\n", bp->free_cnt, bp->cur_buf_ix);
        return (ERR_BUFPOOL_NO_FREE_BUFFER);
    }
}

/*******************************************************************************
 BufPool_ReturnBuffer
*******************************************************************************/
void BufPool_ReturnBuffer(void* bv, unsigned long bu) {
    unsigned long ix;
    unsigned long ba;
    BufPool* bp = (BufPool*) bv;

    // Check that the address of the buffer is reasonable
    if ((bu > (unsigned long) &(bp->buf[POOL_NB_OF_BUFFER - 1][POOL_BUFFER_SIZE - 1])) ||
        (bu < (unsigned long) &(bp->buf[0][0]))) {
        /*
        printf("BufPool_ReturnBuffer: invalid buffer 0x%x (range: 0x%x 0x%x)\r\n",
            bu,
            (unsigned int) &(bp->buf[0][0]),
            (unsigned int) &(bp->buf[POOL_NB_OF_BUFFER-1][POOL_BUFFER_SIZE-1]));
        */
    }

    // Calculate buffer offset from first address
    ba = (bu - ((unsigned long) &(bp->buf[0][0])));

    if (ba % POOL_BUFFER_SIZE) {
        /*
        printf("BufPool_ReturnBuffer: invalid buffer offset %d (buffer 0x%x range: 0x%x 0x%x)\r\n",
            ba,
            bu,
            (unsigned int) &(bp->buf[0][0]),
            (unsigned int) &(bp->buf[POOL_NB_OF_BUFFER-1][POOL_BUFFER_SIZE-1]));
        */
    }

    // Derive buffer index from its address
    ix = ba / POOL_BUFFER_SIZE;

    // Only buffers marked busy can be returned
    if ((bp->busy[ix] & BUFFER_BUSY) == BUFFER_BUSY) {
        bp->busy[ix] = BUFFER_FREE;
        // Make sure the count of free buffer is in range
        if (bp->free_cnt < POOL_NB_OF_BUFFER) {
            bp->free_cnt++;
            /*
            printf("0 BufPool_ReturnBuffer: buf=0x%x ix=%d free_cnt=%d\r\n",
                    (unsigned int) bu,
                    ix,
                    bp->free_cnt);
            */
        } else {
            /*
            printf("BufPool_ReturnBuffer: free buffer overrange %d (buffer 0x%x range: 0x%x 0x%x)\r\n",
                bp->free_cnt,
                (unsigned int) bu,
                (unsigned int) &(bp->buf[0][0]),
                (unsigned int) &(bp->buf[POOL_NB_OF_BUFFER-1][POOL_BUFFER_SIZE-1]));
            */
        }
    } else {
        /*
        printf("BufPool_ReturnBuffer: buffer not busy buf=0x%x ix=%d free_cnt=%d cur_buf_ix=%d flags=0x%x\r\n",
            bu,
            ix,
            bp->free_cnt,
            bp->cur_buf_ix,
            bp->busy[ix]);
        */
    }
}

/*******************************************************************************
 BufPool_GetFreeCnt
*******************************************************************************/
int BufPool_GetFreeCnt(BufPool* bp) {
    return (bp->free_cnt);
}

/*******************************************************************************
 BufPool_GetBufferFlags
*******************************************************************************/
unsigned char BufPool_GetBufferFlags(BufPool* bp, void* bu) {
    unsigned long ix;
    unsigned long ba;

    // Calculate buffer offset from first address
    ba = (((unsigned long) bu) - ((unsigned long) &(bp->buf[0][0])));

    // Derive buffer index from its address
    ix = ba / POOL_BUFFER_SIZE;
    /*
    printf("BufPool_GetBufferFlags: buf=0x%x ix=%d flags=0x%x\r\n", bu, ix, bp->busy[ix]);
    */
    return (bp->busy[ix]);
}
