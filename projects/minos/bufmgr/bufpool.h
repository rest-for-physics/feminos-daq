/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        bufpool.h

 Description: Header file for Buffer Pool entity.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  June 2009: created

  November 2010: added buffer attributes to specify if the buffer will be
  returned to the buffer manager by the user application or by the driver
  to which this buffer will be passed.

*******************************************************************************/
#ifndef BUFPOOL_H
#define BUFPOOL_H

#ifdef JUMBO_POOL
#define POOL_NB_OF_BUFFER 512
#define POOL_BUFFER_SIZE 8192
#else
#define POOL_NB_OF_BUFFER 32
#define POOL_BUFFER_SIZE 2048
#endif
#define POOL_BUFFER_ALIGNMENT 32

// Buffer attribute flags
#define BUFFER_FREE 0
#define BUFFER_BUSY 1
#define AUTO_RETURNED 0
#define USER_RETURNED 2

typedef struct _BufPool {
#ifdef WIN32
    unsigned char buf[POOL_NB_OF_BUFFER][POOL_BUFFER_SIZE];
#elif defined LINUX
    unsigned char buf[POOL_NB_OF_BUFFER][POOL_BUFFER_SIZE];
#else
    unsigned char buf[POOL_NB_OF_BUFFER][POOL_BUFFER_SIZE] __attribute__((aligned(POOL_BUFFER_ALIGNMENT)));
#endif
    unsigned char busy[POOL_NB_OF_BUFFER];
    unsigned int cur_buf_ix;
    unsigned int free_cnt;
} BufPool;

void BufPool_Init(BufPool* bp);
int BufPool_GiveBuffer(BufPool* bp, void** bu, unsigned char flags);
void BufPool_ReturnBuffer(void* bv, unsigned long bu);
int BufPool_GetFreeCnt(BufPool* bp);
unsigned char BufPool_GetBufferFlags(BufPool* bp, void* bu);

#endif
