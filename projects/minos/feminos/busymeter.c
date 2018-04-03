/*******************************************************************************
                           Minos
                           _____________________

 File:        busymeter.c

 Description: Measure performance of DCC:
	- Histogram of BUSY duration


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  October 2011: created from T2K version

  March 2012: removed last null word to make 32-bit word count even; added
  end of frame word that was missing

  January 2014: corrected error on calculation of datagram size that prevented
  END_OF_FRAME to be sent
 
*******************************************************************************/
#include "busymeter.h"
#include "frame.h"

#include <math.h>

// Global variables
unsigned int __attribute__ ((section (".hbusy"))) hbusy[BHISTO_SIZE];
histoint  busy_histogram;

/*******************************************************************************
 Busymeter_PacketizeHisto()
*******************************************************************************/
int Busymeter_PacketizeHisto(histoint *h, void *buf, unsigned int capa, short *sz, int fem_id)
{
	unsigned int *p;
	unsigned short *q;
	int done;
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
	*q = PFX_DEADTIME_HSTAT_BINS;
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

	// The histogram bins
	q = (unsigned short *) p;
	bin_ix = 0;
	done = 0;
	while ((size < (capa-8)) && (done == 0))
	{
		// Put only non null bins
		if (*(h->bins+bin_ix) != 0)
		{
			*q = (unsigned short) PUT_LAT_HISTO_BIN(bin_ix);
			q++;
			*q = (unsigned short) (*(h->bins+bin_ix) & 0xFFFF);
			q++;
			*q = (unsigned short) ((*(h->bins+bin_ix) >> 16)& 0xFFFF);
			q++;
			size +=6;
		}

		// point to next bin
		bin_ix++;
		if (bin_ix == h->bin_cnt)
		{
			done = 1;
		}
	}

	// Add end of frame
	*q = PFX_END_OF_FRAME;
	q++;
	size+=2;

	// Put the final size at the beginning of the buffer, just after the SOF
	q = (unsigned short *) buf;
	q++;
	*q = (unsigned short) size;

	// Include in the size the 2 empty bytes kept at the beginning of the UDP datagram
	size+=2;

	// Return the datagram size to the caller
	*sz = (short) size;

	return(0);
}
