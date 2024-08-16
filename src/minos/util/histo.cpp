/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        histo.c

 Description: Implementation of a generic Histogram object


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  November 2009: created
  August 2012: corrected bugs that were present when min_bin is non zero

*******************************************************************************/
#include "histo.h"
#include "platform_spec.h"
#include <math.h>
#include <cstdio>

/*******************************************************************************
 Histo_Clear()
*******************************************************************************/
void Histo_Clear(histo* h) {
    int i;

    // Clear statistics and set bounds
    h->bin_cnt = ((h->max_bin - h->min_bin) / h->bin_wid) + 1;
    h->min_val = h->max_bin * h->bin_wid;
    h->max_val = h->min_bin * h->bin_wid;
    h->entries = 0;
    h->bin_sat = 0;
    h->align = 0;
    h->mean = 0.0;
    h->stddev = 0.0;

    // Clear all bin content
    for (i = 0; i < h->bin_cnt; i++) {
        *(h->bins + i) = 0;
    }
}

/*******************************************************************************
 Histo_Init()
*******************************************************************************/
void Histo_Init(histo* h, unsigned short min_bin, unsigned short max_bin, unsigned short bin_wid, unsigned short* b) {
    // Save input parameters
    h->min_bin = min_bin;
    h->max_bin = max_bin;
    h->bin_wid = bin_wid;
    h->bins = b;

    // Clear histogram bins
    Histo_Clear(h);
}

/*******************************************************************************
 Histo_AddEntry()
*******************************************************************************/
void Histo_AddEntry(histo* h, unsigned short v) {
    unsigned short val;

    val = (v / h->bin_wid);
    if (val < h->min_bin) val = h->min_bin;
    if (val > h->max_bin) val = h->max_bin;

    val = val - h->min_bin;

    // Increment bin entry count
    if (*(h->bins + val) < 65535) {
        *(h->bins + val) = *(h->bins + val) + 1;
    }
    // We do not increase the entry count and update the min/max count for higher
    // speed. This is done once in the Histo_ComputeStatistics() function
}

/*******************************************************************************
 Histo_ComputeStatistics()
*******************************************************************************/
void Histo_ComputeStatistics(histo* h) {
    int i;

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
            if (*(h->bins + i) == 65535) {
                if (h->bin_sat < 65535) {
                    h->bin_sat++;
                }
            }
        }
    }

    if (h->entries) {
        h->mean = h->mean / h->entries;
        for (i = 0; i < h->bin_cnt; i++) {
            h->stddev = h->stddev + ((*(h->bins + i)) * ((i + h->min_bin) - h->mean) * ((i + h->min_bin) - h->mean));
        }
        h->stddev = sqrt(h->stddev / h->entries);
    }
}

/*******************************************************************************
 Histo_Print()
*******************************************************************************/
void Histo_Print(histo* h) {
    int i;

    for (i = 0; i < h->bin_cnt; i++) {
        printf("Bin (%3d)=%5d\r\n", i, *(h->bins + i));
    }
}
