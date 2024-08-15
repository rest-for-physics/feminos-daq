/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        axi_rbf.c

 Description:


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  May 2011: created
  March 2012: added AxiRingBuffer_GetConfiguration()
  May 2012: allocate AXI buffers on heap instead of a fixed address

*******************************************************************************/

#include "axi_rbf.h"
#include "xparameters.h"
#include <stdio.h>
#include <stdlib.h>

/*******************************************************************************
 AxiRingBuffer_Init
*******************************************************************************/
int AxiRingBuffer_Init(AxiRingBuffer* arbf, unsigned int buf_capa) {
    int i;
    unsigned int bufdesc;
    unsigned char* bufbase;
    unsigned int bufbase_adr;

    // Set control register and I/O FIFO pointers
    arbf->rbf_ctrl = (volatile unsigned int*) (XPAR_LITEAXIFIFO_0_BASEADDR + 0);
    arbf->rbf_fifo = (volatile unsigned int*) (XPAR_LITEAXIFIFO_0_BASEADDR + 4);

    // Say the heap is 24 MB (set in linker script)
    // Allocate 20 MB on heap
    bufbase = malloc(20 * 1024 * 1024);
    bufbase_adr = (unsigned int) bufbase;
    // Take the 8 MSB's of the address
    bufbase_adr = bufbase_adr & 0xFF000000;
    // Add 16 MB
    bufbase_adr = bufbase_adr + 0x01000000;
    // Now bufbase points to allocated memory with at least 4 MB aligned on 16 MB boundary

    // Get Buffer array base address
    // arbf->base = (unsigned int) &(arbf->buf[0][0]);
    // arbf->last = (unsigned int) &(arbf->buf[AXIRBF_NB_OF_BUFFER-1][0]);
    // arbf->base = 0xC5000000;
    // arbf->last = 0xC5100000;
    arbf->base = bufbase_adr;
    arbf->last = bufbase_adr + (AXIRBF_NB_OF_BUFFER - 1) * AXIRBF_BUFFER_SIZE;
    printf("AXI Buffer Base:0x%08x Last:0x%08x\n", arbf->base, arbf->last);

    // Reset engine and setup buffer capacity
    if (buf_capa > AXIRBF_BUFFER_MAX_CAPACITY) {
        printf("AxiRingBuffer_Init: unsupported buffer capacity: %d\n", buf_capa);
        return (-1);
    } else {
        arbf->buffer_capacity = buf_capa;
    }
    *(arbf->rbf_ctrl) = (arbf->base) | (arbf->buffer_capacity) | AXIRBF_RESET;
    *(arbf->rbf_ctrl) = (arbf->base) | (arbf->buffer_capacity);

    // Post buffer descriptors to hardware
    for (i = 0; i < AXIRBF_NB_OF_BUFFER; i++) {
        bufdesc = AXIRBF_BDIX_TO_BD(i, AXIRBF_BUFFER_USER_HW_OFFSET);
        *(arbf->rbf_fifo) = bufdesc;
    }

    return (0);
}

/*******************************************************************************
 AxiRingBuffer_GetFilled
*******************************************************************************/
int AxiRingBuffer_GetFilled(AxiRingBuffer* arbf, unsigned int* bd) {
    unsigned int bufdesc;

    bufdesc = *(arbf->rbf_fifo);
    if (bufdesc == 0xFFFFFFFF) {
        *bd = 0xFFFFFFFF;
    } else {
        *bd = bufdesc;
    }

    return (0);
}

/*******************************************************************************
 AxiRingBuffer_PostFree
*******************************************************************************/
void AxiRingBuffer_PostFree(AxiRingBuffer* arbf, unsigned int buf_adr) {
    unsigned int bd;

    // printf("AxiRingBuffer_PostFree: buffer 0x%08x\n", buf_adr);
    if ((buf_adr > arbf->last) || (buf_adr < arbf->base)) {
        // printf("AxiRingBuffer_PostFree: buffer 0x%08x not in range 0x%08x;0x%08x\n", buf_adr, arbf->base, arbf->last);
        return;
    } else {
        bd = ((buf_adr & 0x00FFE000) << 8) | AXIRBF_BUFFER_USER_HW_OFFSET;
        // printf("AxiRingBuffer_PostFree: returning bd 0x%08x\n", bd);
        *(arbf->rbf_fifo) = bd;
    }
}

/*******************************************************************************
 AxiRingBuffer_IoCtrl
*******************************************************************************/
void AxiRingBuffer_IoCtrl(AxiRingBuffer* arbf, unsigned int bitsel, unsigned int action) {
    *(arbf->rbf_ctrl) = (*(arbf->rbf_ctrl) & (~bitsel)) | (bitsel & action);
}

/*******************************************************************************
 AxiRingBuffer_GetConfiguration
*******************************************************************************/
void AxiRingBuffer_GetConfiguration(AxiRingBuffer* arbf, unsigned int* config) {
    *config = *(arbf->rbf_ctrl);
}
