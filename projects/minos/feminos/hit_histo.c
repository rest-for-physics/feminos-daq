/*******************************************************************************
                           Minos
                           _____________________

 File:        hit_histo.c

 Description:
	Monitoring functions to get the histogram of channel hit count per event


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:

  March 2012: created

  January 2014: corrected error on calculation of datagram size that prevented
  END_OF_FRAME to be sent

*******************************************************************************/
#include "hit_histo.h"
#include "frame.h"

#include <math.h>

// Global variables
unsigned int __attribute__ ((section (".hhit"))) hhitcnt[4][HIT_HISTO_SIZE];
histoint  ch_hit_cnt_histo[4];

/*******************************************************************************
 HitHisto_PacketizeHisto()
*******************************************************************************/
int HitHisto_PacketizeHisto(histoint *h, unsigned short ix, void *buf, unsigned int capa, short *sz, int fem_id)
{
	unsigned int *p;
	unsigned short *q;
	unsigned int size;
	int bin_ix;

	size = 0;

	// Start of Frame
	q = (unsigned short *) buf;
	*q = PUT_FVERSION_FEMID(PFX_START_OF_MFRAME, CURRENT_FRAMING_VERSION, fem_id);
	q++;
	size+=sizeof(unsigned short);
	// Frame size
	*q = 0; // to be changed when the final size is known
	q++;
	size+=sizeof(unsigned short);
	// Histogram header
	*q = PUT_CH_HIT_CNT_HISTO_CHIP_IX(ix);
	q++;
	size+=sizeof(unsigned short);
	*q = PFX_NULL_CONTENT;
	q++;
	size+=sizeof(unsigned short);
	// Histogram summary
	p = (unsigned int *) q;
	*p = h->min_bin;
	p++;
	size+=sizeof(unsigned int);
	*p = h->max_bin;
	p++;
	size+=sizeof(unsigned int);
	*p = h->bin_wid;
	p++;
	size+=sizeof(unsigned int);
	*p = h->bin_cnt;
	p++;
	size+=sizeof(unsigned int);
	*p = h->min_val;
	p++;
	size+=sizeof(unsigned int);
	*p = h->max_val;
	p++;
	size+=sizeof(unsigned int);
	*p = (unsigned int) (100.0 * h->mean);
	p++;
	size+=sizeof(unsigned int);
	*p = (unsigned int) (100.0 * h->stddev);
	p++;
	size+=sizeof(unsigned int);
	*p = h->entries;
	p++;
	size+=sizeof(unsigned int);

	// Put all histogram bins
	for (bin_ix=0; bin_ix<(h->bin_cnt); bin_ix++)
	{
		*p = *(h->bins + bin_ix);
		p++;
		size+=sizeof(unsigned int);

		// Do not fill more than buffer capacity
		if (size > (capa-16))
		{
			break;
		}
	}

	// Add end of frame
	q = (unsigned short *) p;
	*q = PFX_END_OF_FRAME;
	q++;
	size+=2;

	// Put the final size at the beginning of the buffer, just after the SOF
	q = (unsigned short *) buf;
	q++;
	*q = (unsigned short) size;

	// Include in the size the 2 empty bytes kept at the beginning of the UDP datagram
	size+=2;

	// Return the size of the datagram to the caller
	*sz = (short) size;

	return(0);
}
