/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        pedestal.h

 Description: Definition of functions to monitor pedestals


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2011: created from T2K version

*******************************************************************************/
#ifndef _PEDESTAL_H
#define _PEDESTAL_H

#define PHISTO_SIZE 512

#include "cmdi.h"
#include "histo.h"

typedef struct _phisto {
    int stat_valid; // private field to indicate if histogram statistics are up to date
    histo ped_histo;
    unsigned short ped_bins[PHISTO_SIZE];
} phisto;

int Pedestal_ScanDFrame(void* fr, int sz);

void Pedestal_UpdateHisto(unsigned short asic,
                          unsigned short channel,
                          unsigned short samp);

int Pedestal_ClearHisto(unsigned short asic, unsigned short channel);

int Pedestal_SetHistoOffset(unsigned short asic, unsigned short channel, unsigned short offset);

int Pedestal_IntrepretCommand(CmdiContext* c);

void Pedestal_PacketizeHisto(CmdiContext* c, unsigned short as, unsigned short ch);

#endif
