/*******************************************************************************

                           _____________________

 File:        axi_rbf.h

 Description:


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  May 2011: created

*******************************************************************************/

#ifndef _AXI_RBF_H
#define _AXI_RBF_H

#define AXIRBF_NB_OF_BUFFER 128
#define AXIRBF_BUFFER_SIZE 8192
#define AXIRBF_BUFFER_ALIGNMENT 0x01000000

#define AXIRBF_BUFFER_MAX_CAPACITY 8192
#define AXIRBF_BUFFER_USER_SW_OFFSET 44
#define AXIRBF_BUFFER_USER_HW_OFFSET 48

#define AXIRBF_CAPACITY_MASK 0x00003FFF
#define AXIRBF_IOCONTROL_MASK 0x003F0000
#define AXIRBF_STOP 0x00000000
#define AXIRBF_RESET 0x00010000
#define AXIRBF_RUN 0x00020000
#define AXIRBF_RETPND 0x00040000
#define AXIRBF_TIMED 0x00080000
#define AXIRBF_TIMEVAL_MASK 0x00300000
#define AXIRBF_BASE_MASK 0xFF000000

#define AXIRBF_GET_CAPACITY(rb) ((rb & AXIRBF_CAPACITY_MASK) >> 0)
#define AXIRBF_GET_RUN(rb) ((rb & AXIRBF_RUN) >> 17)
#define AXIRBF_GET_RETPND(rb) ((rb & AXIRBF_RETPND) >> 18)
#define AXIRBF_GET_TIMED(rb) ((rb & AXIRBF_TIMED) >> 19)
#define AXIRBF_PUT_TIMEVAL(rb, tv) ((rb & (~AXIRBF_TIMEVAL_MASK)) | ((tv << 20) & AXIRBF_TIMEVAL_MASK))
#define AXIRBF_GET_TIMEVAL(rb) ((rb & AXIRBF_TIMEVAL_MASK) >> 20)
#define AXIRBF_GET_BASE(rb) ((rb & AXIRBF_BASE_MASK) >> 0)

typedef struct _AxiRingBuffer {
    // unsigned char buf[AXIRBF_NB_OF_BUFFER][AXIRBF_BUFFER_SIZE];
    unsigned int base;
    unsigned int last;
    unsigned int buffer_capacity;
    volatile unsigned int* rbf_fifo;
    volatile unsigned int* rbf_ctrl;

} AxiRingBuffer;

#define AXIRBF_BDIX_TO_BD(ix, uo) ((((ix) * AXIRBF_BUFFER_SIZE) << 8) | (uo))
#define AXIRBF_GET_BDADDR(ar, bu) (((ar)->base) | (((bu) & 0xFFE00000) >> 8))
#define AXIRBF_GET_BDSIZE(bu) (((bu) & 0x00001FFF) - AXIRBF_BUFFER_USER_SW_OFFSET)

int AxiRingBuffer_Init(AxiRingBuffer* arbf, unsigned int buf_capa);
int AxiRingBuffer_GetFilled(AxiRingBuffer* arbf, unsigned int* bd);
void AxiRingBuffer_PostFree(AxiRingBuffer* arbf, unsigned int buf_adr);
void AxiRingBuffer_IoCtrl(AxiRingBuffer* arbf, unsigned int bitsel, unsigned int action);
void AxiRingBuffer_GetConfiguration(AxiRingBuffer* arbf, unsigned int* config);

#endif //_AXI_RBF_H
