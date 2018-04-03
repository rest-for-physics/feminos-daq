/*******************************************************************************

                           _____________________

 File:        histo_int.h

 Description: A generic Histogram object
	version in unsigned integer

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  June 2011: created from T2K version
 
*******************************************************************************/
#ifndef _HISTOINT_H
#define _HISTOINT_H

typedef struct _histoint {
	unsigned int   min_bin;    // range lower bound
	unsigned int   max_bin;    // range higher bound
	unsigned int   bin_wid;    // width of one bin in number of units
	unsigned int   bin_cnt;    // number of bins
	unsigned int   min_val;    // lowest bin with at least one entry
	unsigned int   max_val;    // highest bin with at least one entry
	unsigned int   entries;    // number of entries
	unsigned int   bin_sat;    // number of bins saturated
	float          mean;       // mean value
	float          stddev;     // standard deviation
	unsigned int  *bins;	   // pointer to array of bins
} histoint;

void HistoInt_Init(histoint *h, unsigned int min_bin, unsigned int max_bin, unsigned int bin_wid, unsigned int *b);
void HistoInt_Clear(histoint *h);
void HistoInt_AddEntry(histoint *h, unsigned int v);
void HistoInt_ComputeStatistics(histoint *h);
void HistoInt_Print(histoint *h, int v);
void HistoInt_PrintStat(histoint *h);

#endif

