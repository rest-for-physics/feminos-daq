/*******************************************************************************

                           _____________________

 File:        busymeter.h

 Description: Definition of functions


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  June 2011: created from T2K version
 
*******************************************************************************/
#ifndef _BUSYMETER_H
#define _BUSYMETER_H

#include "histo_int.h"

#define BHISTO_SIZE 1024

// Global variables declared in busymeter.c
extern unsigned int __attribute__ ((section (".hbusy"))) hbusy[BHISTO_SIZE];
extern histoint  busy_histogram;

// Histogram data packet
typedef struct _HistogramPacket {
	unsigned short hdr ;     // header prefix
	unsigned short min_bin;  // range lower bound
	unsigned short max_bin;  // range higher bound
	unsigned short bin_wid;  // width of one bin in number of units
	unsigned short bin_cnt;  // number of bins
	unsigned short min_val;  // lowest bin with at least one entry
	unsigned short max_val;  // highest bin with at least one entry
	unsigned short mean;     // mean
	unsigned short stddev;   // standard deviation
	unsigned short pad;      // padding for 32-bit alignment
	unsigned int   entries;  // number of entries
} HistogramPacket;

int Busymeter_PacketizeHisto(histoint *h, void *buf, unsigned int capa, short *sz, int fem_id);

#endif

