/*******************************************************************************

                           _____________________

 File:        histo_int.c

 Description: Implementation of a generic Histogram object
  unsigned integer version


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  June 2011: created from T2K version

*******************************************************************************/
#include "histo_int.h"
#include <math.h>
#include <stdio.h>

/*******************************************************************************
 HistoInt_Clear()
*******************************************************************************/
void HistoInt_Clear(histoint* h) {
    unsigned int i;

    // Clear statistics and set bounds
    h->bin_cnt = ((h->max_bin - h->min_bin) / h->bin_wid) + 1;
    h->min_val = h->max_bin * h->bin_wid;
    h->max_val = h->min_bin * h->bin_wid;
    h->entries = 0;
    h->bin_sat = 0;
    h->mean = 0.0;
    h->stddev = 0.0;

    // Clear all bin content
    for (i = 0; i < h->bin_cnt; i++) {
        *(h->bins + i) = 0;
    }
}

/*******************************************************************************
 HistoInt_Init()
*******************************************************************************/
void HistoInt_Init(histoint* h, unsigned int min_bin, unsigned int max_bin, unsigned int bin_wid, unsigned int* b) {
    // Save input parameters
    h->min_bin = min_bin;
    h->max_bin = max_bin;
    h->bin_wid = bin_wid;
    h->bins = b;

    // Clear histogram bins
    HistoInt_Clear(h);
}

/*******************************************************************************
 HistoInt_AddEntry()
*******************************************************************************/
void HistoInt_AddEntry(histoint* h, unsigned int v) {
    unsigned int val;

    if (v > h->max_bin) {
        val = h->max_bin / h->bin_wid;
    } else {
        val = v / h->bin_wid;
    }

    // Increment bin entry count
    if (*(h->bins + val) != 0xFFFFFFFF) {
        *(h->bins + val) = *(h->bins + val) + 1;
    }
    // We do not increase the entry count and update the min/max count for higher
    // speed. This is done once in the Histo_ComputeStatistics() function
}

/*******************************************************************************
 HistoInt_ComputeStatistics()
*******************************************************************************/
void HistoInt_ComputeStatistics(histoint* h) {
    unsigned int i;

    h->entries = 0;
    h->mean = 0.0;
    h->stddev = 0.0;

    for (i = 0; i < h->bin_cnt; i++) {
        h->entries += *(h->bins + i);
        h->mean = h->mean + ((i + h->min_bin) * (*(h->bins + i)) * h->bin_wid);

        if (*(h->bins + i)) {
            if (((i + h->min_bin) * h->bin_wid) > h->max_val) {
                h->max_val = (i + h->min_bin) * h->bin_wid;
            }
            if (((i + h->min_bin) * h->bin_wid) < h->min_val) {
                h->min_val = (i + h->min_bin) * h->bin_wid;
            }
            if (*(h->bins + i) == 0xFFFFFFFF) {
                if (h->bin_sat < 0xFFFFFFFF) {
                    h->bin_sat++;
                }
            }
        }
    }

    if (h->entries) {
        h->mean = h->mean / h->entries;
        for (i = 0; i < h->bin_cnt; i++) {
            h->stddev = h->stddev + ((*(h->bins + i)) * (i - h->mean) * (i - h->mean));
        }
        h->stddev = sqrt(h->stddev / h->entries);
    }
}

/*******************************************************************************
 HistoInt_Print()
*******************************************************************************/
void HistoInt_Print(histoint* h, int v) {
    unsigned int i;

    for (i = 0; i < h->bin_cnt; i++) {
        if ((v == 1) || (*(h->bins + i) != 0)) {
            printf("Bin (%3d)=%5d\n", i, *(h->bins + i));
        }
    }
}

/*******************************************************************************
 HistoInt_PrintStat()
*******************************************************************************/
void HistoInt_PrintStat(histoint* h) {
    printf("Bin min   : %d\n", h->min_bin);
    printf("Bin max   : %d\n", h->max_bin);
    printf("Bin width : %d\n", h->bin_wid);
    printf("Bin count : %d\n", h->bin_cnt);
    printf("Min val   : %d\n", h->min_val);
    printf("Max val   : %d\n", h->max_val);
    printf("Mean      : %.2f\n", h->mean);
    printf("StdDev    : %.2f\n", h->stddev);
    printf("Entries   : %d\n", h->entries);
}
