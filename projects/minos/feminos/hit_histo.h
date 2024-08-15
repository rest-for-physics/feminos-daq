/*******************************************************************************

                           _____________________

 File:        hit_histo.h

 Description: Definitions related to Channel Hit Count Histograms


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  Mard 2012: created

*******************************************************************************/
#ifndef _HIT_HISTO_H
#define _HIT_HISTO_H

#include "histo_int.h"

#define HIT_HISTO_SIZE 128

// Global variables declared in hit_histo.c
extern unsigned int __attribute__((section(".hhit"))) hhitcnt[4][HIT_HISTO_SIZE];
extern histoint ch_hit_cnt_histo[4];

// Channel Hit Count Histogram data packet
typedef struct _ChannelHitCountHistoPacket {
    unsigned short hdr;     // header prefix
    unsigned short min_bin; // range lower bound
    unsigned short max_bin; // range higher bound
    unsigned short bin_wid; // width of one bin in number of units
    unsigned short bin_cnt; // number of bins
    unsigned short min_val; // lowest bin with at least one entry
    unsigned short max_val; // highest bin with at least one entry
    unsigned short mean;    // mean
    unsigned short stddev;  // standard deviation
    unsigned short pad;     // padding for 32-bit alignment
    unsigned int entries;   // number of entries
} ChannelHitCountHistoPacket;

int HitHisto_PacketizeHisto(histoint* h, unsigned short ix, void* buf, unsigned int capa, short* sz, int fem_id);

#endif
