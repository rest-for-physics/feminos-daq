/*******************************************************************************

                           _____________________

 File:        periodmeter.h

 Description: Definition of functions


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  February 2013: created from busymeter
 
*******************************************************************************/
#ifndef _PERIODMETER_H
#define _PERIODMETER_H

#include "histo_int.h"

#define EHISTO_SIZE 1024

// Global variables declared in periodmeter.c
extern unsigned int __attribute__ ((section (".hevper"))) hevper[EHISTO_SIZE];
extern histoint  evper_histogram;


int Periodmeter_PacketizeHisto(histoint *h, void *buf, unsigned int capa, short *sz, int fem_id);

#endif

