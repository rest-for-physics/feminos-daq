/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        hitscurve.h

 Description: Definition of functions to work on AGET threshold S-curves


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  December 2011: created

*******************************************************************************/
#ifndef _HITSCURVE_H
#define _HITSCURVE_H

#define SHISTO_SIZE 18

#include "cmdi.h"
#include "feminos.h"
#include "histo.h"

/* S-histogram for one channel */
typedef struct _Shisto {
    histo s_histo;
    unsigned short s_bins[SHISTO_SIZE];
} Shisto;

/* Array of S-histograms for one ASIC */
typedef struct _AsicShisto {
    Shisto shisto[ASIC_MAX_CHAN_INDEX + 1];
} AsicShisto;

/* Array of S-hitograms for one Feminos card */
typedef struct _FeminosShisto {
    int cur_thr;
    AsicShisto asicshisto[MAX_NB_OF_ASIC_PER_FEMINOS];
} FeminosShisto;

/* The following is defined in a global variable */
extern FeminosShisto fshisto;

/* Function prototypes */
int FeminosShisto_ClearAll(FeminosShisto* fs);
int FeminosShisto_ScanDFrame(void* fr, int sz);
int FeminosShisto_IntrepretCommand(CmdiContext* c);
void FeminosShisto_Packetize(CmdiContext* c,
                             unsigned short as,
                             unsigned short ae,
                             unsigned short ch,
                             unsigned short ce);

#endif
