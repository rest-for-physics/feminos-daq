/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        histo.h

 Description: A generic Histogram object


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  November 2009: created

*******************************************************************************/
#ifndef _HISTO_H
#define _HISTO_H

typedef struct _histo {
    unsigned short min_bin; // range lower bound
    unsigned short max_bin; // range higher bound
    unsigned short bin_wid; // width of one bin in number of units
    unsigned short bin_cnt; // number of bins
    unsigned short min_val; // lowest bin with at least one entry
    unsigned short max_val; // highest bin with at least one entry
    unsigned int entries;   // number of entries
    unsigned short bin_sat; // number of bins saturated
    unsigned short align;   // dummy word for alignement
    float mean;             // mean value
    float stddev;           // standard deviation
    unsigned short* bins;   // pointer to array of bins
} histo;

void Histo_Init(histo* h, unsigned short min_bin, unsigned short max_bin, unsigned short bin_wid, unsigned short* b);
void Histo_Clear(histo* h);
void Histo_AddEntry(histo* h, unsigned short v);
void Histo_ComputeStatistics(histo* h);
void Histo_Print(histo* h);

#endif
