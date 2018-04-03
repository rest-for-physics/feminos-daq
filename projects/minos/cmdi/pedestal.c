/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        pedestal.c

 Description: Implementation of functions to monitor pedestals


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  October 2011: created from T2K version
  August 2012: added command to set histogram offset
   changed threshold calculation to work with positive or negative detector
   polarity

  January 2014: corrected error on calculation of datagram size that prevented
  END_OF_FRAME to be sent
  
  March 2014: changed variable polarity from scalar to vector

*******************************************************************************/
#include "pedestal.h"
#include "feminos.h"
#include "cmdi.h"
#include "after.h"
#include "aget.h"
#include "frame.h"
#include "error_codes.h"

#include <stdio.h>
#include <string.h>

#define HISTO_ALLOWED_SIZE       1400

#define DEFAULT_PED_TARGET_MEAN   250
#define DEFAULT_THR_STDDEV_FACTOR 3.5

// Global variable
#ifdef HAS_PEDESTAL_HISTOGRAMS
extern phisto pedestal_histogram[MAX_NB_OF_ASIC_PER_FEMINOS][ASIC_MAX_CHAN_INDEX+1];
#endif

/*******************************************************************************
 Pedestal_PacketizeHisto 
*******************************************************************************/
void Pedestal_PacketizeHisto(CmdiContext *c, unsigned short as, unsigned short ch)
{
#ifdef HAS_PEDESTAL_HISTOGRAMS
	int i;
	unsigned short size;
	unsigned short *p;

	size = 0;
	p = (unsigned short *) (c->burep);
	c->reply_is_cframe = 0;

	// Put Start Of Frame + Version
	*p = PUT_FVERSION_FEMID(PFX_START_OF_MFRAME, CURRENT_FRAMING_VERSION, c->fem->card_id);
	p++;
	size+=2;

	// Leave space for size;
	p++;
	size+=2;

	// Put Card/Chip/Channel index for Pedestal Histo
	*p = PUT_CARD_CHIP_CHAN_HISTO(c->fem->card_id, as, ch);
	p++;
	size+=2;

	// Put all non-null content pedestal histogram bins
	for (i=0; i<PHISTO_SIZE; i++)
	{
		if (pedestal_histogram[as][ch].ped_bins[i] != 0)
		{
			*p = PUT_HISTO_BIN_IX(i);
			p++;
			*p = pedestal_histogram[as][ch].ped_bins[i];
			p++;
			size+=4;
		}
		if (size >= HISTO_ALLOWED_SIZE) break;
	}

	// Put End Of Frame
	*p = PFX_END_OF_FRAME;
	p++;
	size+=2;

	// Put packet size after start of frame
	p = (unsigned short *) (c->burep);
	p++;
	*p = size;

	// Include in the size the 2 empty bytes kept at the beginning of the UDP datagram
	size+=2;
	c->rep_size = size;

#endif
}

/*******************************************************************************
 Pedestal_PacketizeHistoSummary
*******************************************************************************/
void Pedestal_PacketizeHistoSummary(CmdiContext *c,
		unsigned short as_b,
		unsigned short ch_b,
		unsigned short as_e,
		unsigned short ch_e)
{
#ifdef HAS_PEDESTAL_HISTOGRAMS

	unsigned short size;
	unsigned short *p;
	unsigned int   *q;
	histo          *h;
	unsigned short as;
	unsigned short ch;

	size = 0;
	p = (unsigned short *) (c->burep);
	c->reply_is_cframe = 0;

	// Put Start Of Frame + Version
	*p = PUT_FVERSION_FEMID(PFX_START_OF_MFRAME, CURRENT_FRAMING_VERSION, c->fem->card_id);
	p++;
	size+=2;

	// Leave space for size;
	p++;
	size+=sizeof(unsigned short);

	for (as = as_b; as <= as_e; as++)
	{
		for (ch = ch_b; ch <= ch_e; ch++)
		{
			h = &(pedestal_histogram[as][ch].ped_histo);

			// Put Card/Chip/Channel index for Pedestal Histogram
			*p = PUT_CARD_CHIP_CHAN_HISTO(c->fem->card_id, as, ch);
			p++;
			size+=sizeof(unsigned short);

			// Histogram header
			*p = PFX_PEDESTAL_H_MD;
			p++;
			size+=sizeof(unsigned short);

			// Histogram Condensed Summary
			q = (unsigned int *) p;
			*q = (unsigned int) (100.0 * h->mean);
			q++;
			size+=sizeof(unsigned int);
			*q = (unsigned int) (100.0 * h->stddev);
			q++;
			size+=sizeof(unsigned int);

			p = (unsigned short *) q;

			if (size >= HISTO_ALLOWED_SIZE) break;
		}
		if (size >= HISTO_ALLOWED_SIZE) break;
	}

	// Put End Of Frame
	*p = PFX_END_OF_FRAME;
	p++;
	size+=sizeof(unsigned short);

	// Put packet size after start of frame
	p = (unsigned short *) (c->burep);
	p++;
	*p = size;

	// Include in the size the 2 empty bytes kept at the beginning of the UDP datagram
	size+=2;
	c->rep_size = size;

#endif
}

/*******************************************************************************
 Pedestal_PacketizeHistoMath
*******************************************************************************/
void Pedestal_PacketizeHistoMath(CmdiContext *c, unsigned short as, unsigned short ch)
{
#ifdef HAS_PEDESTAL_HISTOGRAMS
	unsigned short size;
	unsigned short *p;
	unsigned int   *q;
	histo          *h;

	h = &(pedestal_histogram[as][ch].ped_histo);

	size = 0;
	p = (unsigned short *) (c->burep);
	c->reply_is_cframe = 0;

	// Put Start Of Frame + Version
	*p = PUT_FVERSION_FEMID(PFX_START_OF_MFRAME, CURRENT_FRAMING_VERSION, c->fem->card_id);
	p++;
	size+=2;

	// Leave space for size;
	p++;
	size+=sizeof(unsigned short);

	// Put Card/Chip/Channel index for Pedestal Histogram
	*p = PUT_CARD_CHIP_CHAN_HISTO(c->fem->card_id, as, ch);
	p++;
	size+=sizeof(unsigned short);

	// Histogram header
	*p = PFX_PEDESTAL_HSTAT;
	p++;
	size+=sizeof(unsigned short);

	// Histogram summary
	q = (unsigned int *) p;
	*q = h->min_bin;
	q++;
	size+=sizeof(unsigned int);
	*q = h->max_bin;
	q++;
	size+=sizeof(unsigned int);
	*q = h->bin_wid;
	q++;
	size+=sizeof(unsigned int);
	*q = h->bin_cnt;
	q++;
	size+=sizeof(unsigned int);
	*q = h->min_val;
	q++;
	size+=sizeof(unsigned int);
	*q = h->max_val;
	q++;
	size+=sizeof(unsigned int);
	*q = (unsigned int) (100.0 * h->mean);
	q++;
	size+=sizeof(unsigned int);
	*q = (unsigned int) (100.0 * h->stddev);
	q++;
	size+=sizeof(unsigned int);
	*q = h->entries;
	q++;
	size+=sizeof(unsigned int);

	p = (unsigned short *) q;
	// Put End Of Frame
	*p = PFX_END_OF_FRAME;
	p++;
	size+=sizeof(unsigned short);

	// Put packet size after start of frame
	p = (unsigned short *) (c->burep);
	p++;
	*p = size;

	// Include in the size the 2 empty bytes kept at the beginning of the UDP datagram
	size+=2;
	c->rep_size = size;

#endif
}

/*******************************************************************************
 Pedestal_ComputeHistoMath 
*******************************************************************************/
int Pedestal_ComputeHistoMath(unsigned short asic, unsigned short channel)
{
#ifdef HAS_PEDESTAL_HISTOGRAMS

	if ( (asic <MAX_NB_OF_ASIC_PER_FEMINOS) && (channel <= ASIC_MAX_CHAN_INDEX) )
	{
		// Compute histogram statistics if they are not up to date
		if (pedestal_histogram[asic][channel].stat_valid == 0)
		{
			Histo_ComputeStatistics(&(pedestal_histogram[asic][channel].ped_histo));
			pedestal_histogram[asic][channel].stat_valid = 1;
		}
		return (0);
	}
	else
#endif
	{
		return (ERR_ILLEGAL_PARAMETER);
	}
}

/*******************************************************************************
 Pedestal_IntrepretCommand
*******************************************************************************/
int Pedestal_IntrepretCommand(CmdiContext *c)
{
	int err = -1;
	
#ifdef HAS_PEDESTAL_HISTOGRAMS

	int param[10];
	int hcnt;
	unsigned short  asic_cur, asic_beg, asic_end;
	unsigned short  cha_cur, cha_beg, cha_end;
	char str[8][16];
	short ped_uf, ped_of, ped_shift, ped_target, ped_thr;
	float stdev_factor;
	int arg_cnt;
	char *rep;
	int asic_max_chan_index;
	
	rep = (char *)( c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME
	err = 0;

	// Compute ASIC max channel index
	if (c->mode == 1)
	{
		asic_max_chan_index = MAX_CHAN_INDEX_AFTER;
	}
	else
	{
		asic_max_chan_index = MAX_CHAN_INDEX_AGET;
	}

	// Init
	asic_beg = 0;
	asic_end = 0;
	cha_beg  = 0;
	cha_end  = 0;
	
	// Scan arguments
	if ((arg_cnt = sscanf(c->cmd, "hped %s %s %s %s %s", &str[0][0], &str[1][0], &str[2][0], &str[3][0], &str[4][0])) >= 3)
	{
		// Wildcard on ASIC?
		if (str[1][0] == '*')
		{
			asic_beg = 0;
			asic_end = MAX_NB_OF_ASIC_PER_FEMINOS-1;
		}
		else
		{
			// Scan ASIC index range
			if (sscanf(&str[1][0], "%d:%d", &param[0], &param[1]) == 2)
			{
				if (param[0] < param[1])
				{
					asic_beg = (unsigned short) param[0];
					asic_end = (unsigned short) param[1];
				}
				else
				{
					asic_beg = (unsigned short) param[1];
					asic_end = (unsigned short) param[0];
				}
			}
			// Scan ASIC index
			else if (sscanf(&str[1][0], "%d", &param[0]) == 1)
			{
				asic_beg = (unsigned short) param[0];
				asic_end = (unsigned short) param[0];
			}
			else
			{
				err = ERR_SYNTAX;
			}
		}

		// Wildcard on Channel?
		if (str[2][0] == '*')
		{
			cha_beg = 0;
			cha_end = asic_max_chan_index;
		}
		else
		{
			// Scan Channel index range
			if (sscanf(&str[2][0], "%d:%d", &param[0], &param[1]) == 2)
			{
				if (param[0] < param[1])
				{
					cha_beg = (unsigned short) param[0];
					cha_end = (unsigned short) param[1];
				}
				else
				{
					cha_beg = (unsigned short) param[1];
					cha_end = (unsigned short) param[0];
				}
			}
			// Scan Channel index
			else if (sscanf(&str[2][0], "%d", &param[0]) == 1)
			{
				cha_beg = (unsigned short) param[0];
				cha_end = (unsigned short) param[0];
			}
			else
			{
				err = ERR_SYNTAX;
			}
		}

		// Return if any syntax error was found
		if (err == ERR_SYNTAX)
		{
			sprintf(rep, "%d Fem(%02d) hped <action> <ASIC> <Channel>\n", err, c->fem->card_id);
			c->do_reply = 1;
			return (err);
		}
			
		// Check that supplied arguments are within acceptable range
		if (!(
			(asic_beg <  MAX_NB_OF_ASIC_PER_FEMINOS) &&
			(cha_beg  <= asic_max_chan_index) &&
			(asic_end <  MAX_NB_OF_ASIC_PER_FEMINOS) &&
			(cha_end  <= asic_max_chan_index)
			))
		{
			err = ERR_ILLEGAL_PARAMETER;
			sprintf(rep, "%d Fem(%02d): hped %s %d %d\n", err, c->fem->card_id, &str[0][0], asic_beg, cha_beg);
			c->do_reply = 1;
			return (err);
		}
		
		// Scan optional arguments
		if ((arg_cnt >= 4) && (sscanf(&str[3][0], "%d", &param[0]) == 1))
		{
			ped_target = (unsigned short) param[0];
		}
		else
		{
			ped_target = DEFAULT_PED_TARGET_MEAN;
		}
		if ((arg_cnt == 5) && (sscanf(&str[4][0], "%f", &stdev_factor) == 1))
		{
			// stdev_factor already assigned
		}
		else
		{
			stdev_factor = DEFAULT_THR_STDDEV_FACTOR;
		}
	}
	else
	{
		err = ERR_SYNTAX;
		c->do_reply = 1;
		sprintf(rep, "%d Fem(%02d): hped <action> <ASIC> <Channel>\n", err, c->fem->card_id);
		return (err);
	}
		
	// Command to return histogram to DAQ
	if (strncmp(&str[0][0], "get", 3) == 0)
	{
		// Fill buffer with pedestal histogram bins of 1 channel
		if (strncmp(&str[0][0], "getbins", 7) == 0)
		{
			err = 0;
			Pedestal_PacketizeHisto(c, asic_beg, cha_beg);
			c->do_reply = 1;
		}
		// Fill buffer with histogram statistics of 1 channel
		else if (strncmp(&str[0][0], "getmath", 7) == 0)
		{
			Pedestal_ComputeHistoMath(asic_beg, cha_beg);
			Pedestal_PacketizeHistoMath(c, asic_beg, cha_beg);
			c->do_reply = 1;
		}
		// Fill buffer with histogram statistics of all channels of one/several ASICs
		else if (strncmp(&str[0][0], "getsummary", 10) == 0)
		{
			for (asic_cur= asic_beg; asic_cur<= asic_end; asic_cur++)
			{
				for (cha_cur= cha_beg; cha_cur<= cha_end; cha_cur++)
				{
					Pedestal_ComputeHistoMath(asic_cur, cha_cur);
				}
			}
			Pedestal_PacketizeHistoSummary(c, asic_beg, cha_beg, asic_end, cha_end);
			c->do_reply = 1;
		}
		else
		{
			err = ERR_SYNTAX;
			c->do_reply = 1;
			sprintf(rep, "%d Fem(%02d): unknown command: hped %s\n", err, c->fem->card_id, &str[0][0]);
			return (err);
		}
		return (err);
	}

	// For other commands, loop on desired operation
	err    = 0;
	hcnt   = 0;
	ped_uf = 0;
	ped_of = 0;
	for (asic_cur= asic_beg; asic_cur<= asic_end; asic_cur++)
	{
		for (cha_cur= cha_beg; cha_cur<= cha_end; cha_cur++)
		{
			// Command to clear histograms
			if (strncmp(&str[0][0], "clr", 3) == 0)
			{
				if ( (err = Pedestal_ClearHisto(asic_cur, cha_cur)) < 0)
				{
					sprintf(rep, "%d Fem(%02d): Pedestal_ClearHisto failed on Asic %d Channel %d\n",
						err,
						c->fem->card_id,
						asic_cur,
						cha_cur);
					c->do_reply = 1;
					return(err);
				}
				else
				{
					hcnt++;	
				}
			}
					
			// Command to set histogram offset
			else if (strncmp(&str[0][0], "offset", 6) == 0)
			{
				if ( (err = Pedestal_SetHistoOffset(asic_cur, cha_cur, ped_target)) < 0)
				{
					sprintf(rep, "%d Fem(%02d): Pedestal_SetHistoOffset failed on Asic %d Channel %d\n",
						err,
						c->fem->card_id,
						asic_cur,
						cha_cur);
					c->do_reply = 1;
					return(err);
				}
				else
				{
					hcnt++;
				}
			}

			// Command to center pedestals histograms around a given value (a.k.a. pedestal equalization)
			else if (strncmp(&str[0][0], "centermean", 10) == 0)
			{
				// Compute histogram statistics (does nothing if up to date)
				Pedestal_ComputeHistoMath(asic_cur, cha_cur);
					
				// Calculate correction value
				ped_shift =  ped_target - (short)(pedestal_histogram[asic_cur][cha_cur].ped_histo.mean + 0.5);
				if (ped_shift < -256)
				{
					ped_shift = -256;
					ped_uf++;
				}
				if (ped_shift > 255)
				{
					ped_shift = 255;
					ped_of++;
				}
						
				// Write pedestal equalization value to pedestal table
				pedthrlut[asic_cur][cha_cur].ped = ped_shift;
				hcnt++;	
			}
					
			// Command to set channel threshold at some level above pedestal + Factor * stddev
			else if (strncmp(&str[0][0], "setthr", 6) == 0)
			{
				// Compute histogram statistics (does nothing if up to date)
				Pedestal_ComputeHistoMath(asic_cur, cha_cur);

				// Calculate threshold value
				if (c->polarity[asic_cur] == 0)
				{
					ped_thr =  ped_target +
						(short)(stdev_factor * pedestal_histogram[asic_cur][cha_cur].ped_histo.stddev + 0.5);
					if (ped_thr > 511)
					{
						ped_thr = 511;
						ped_of++;
					}
				}
				else
				{
					ped_thr =  ped_target -
						(short)(stdev_factor * pedestal_histogram[asic_cur][cha_cur].ped_histo.stddev + 0.5);
					if (ped_thr < 0)
					{
						ped_thr = 0;
						ped_uf++;
					}
				}
						
				// Write threshold value to threshold table
				pedthrlut[asic_cur][cha_cur].thr = ped_thr;
				hcnt++;	
			}
				
			else
			{
				err = ERR_SYNTAX;
				c->do_reply = 1;
				sprintf(rep, "%d Fem(%02d): unknown command: hped %s\n", err, c->fem->card_id, &str[0][0]);
				return (err);
			}
		}		
	}		

	// All actions are now done 
	sprintf(rep, "%d Fem(%02d): hped %s done on %d histograms. Underflow: %d Overflow: %d\n",
		err,
		c->fem->card_id,
		&str[0][0],
		hcnt,
		ped_uf,
		ped_of);
	c->do_reply = 1;
#endif
	return(err);
}

/*******************************************************************************
 Pedestal_UpdateHisto
*******************************************************************************/
void Pedestal_UpdateHisto(unsigned short asic,
						  unsigned short channel,
						  unsigned short samp)
{
#ifdef HAS_PEDESTAL_HISTOGRAMS

	// Update histogram
	if ((asic < MAX_NB_OF_ASIC_PER_FEMINOS) && (channel <= MAX_CHAN_INDEX_AFTER))
	{
		Histo_AddEntry(&(pedestal_histogram[asic][channel].ped_histo), samp);
		// Mark histogram statistics invalid
		pedestal_histogram[asic][channel].stat_valid = 0;
	}

#endif
}

/*******************************************************************************
 Pedestal_ScanDFrame
*******************************************************************************/
int Pedestal_ScanDFrame(void *fr, int sz)
{
#ifdef HAS_PEDESTAL_HISTOGRAMS
	unsigned short *p;
	int i, j;
	int sz_rd;
	int done = 0;
	unsigned short r0, r1, r2;
	int si;
	char *c;
	unsigned int *ui;
	int err;

	p = (unsigned short *) fr;

	done  = 0;
	i     = 0;
	sz_rd = 0;
	si    = 0;

	r0 = 0;
	r1 = 0;
	r2 = 0;

	err = 0;

	while (!done)
	{
		// Is it a prefix for 14-bit content?
		if ((*p & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_IX)
		{
			r0 = GET_CARD_IX(*p);
			r1 = GET_CHIP_IX(*p);
			r2 = GET_CHAN_IX(*p);
			//printf("Card %02d Chip %01d Channel %02d\n", r0, r1, r2);
			i++;
			p++;
			sz_rd+=2;
			si = 0;
			//printf("PFX_CARD_CHIP_CHAN_HIT_IX\n");
		}
		else if ((*p & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_CNT)
		{
			i++;
			p++;
			sz_rd+=2;
			//printf("PFX_CARD_CHIP_CHAN_HIT_CNT\n");
		}
		else if ((*p & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HISTO)
		{
			i++;
			p++;
			sz_rd+=2;
			//printf("PFX_CARD_CHIP_CHAN_HISTO\n");
		}
		// Is it a prefix for 12-bit content?
		else if ((*p & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE)
		{
			r0 = GET_ADC_DATA(*p);
			i++;
			p++;
			sz_rd+=2;
			si++;
			Pedestal_UpdateHisto(r1, r2, r0);
			//printf("ADC 0x%x\n", r0);
		}
		else if ((*p & PFX_12_BIT_CONTENT_MASK) == PFX_LAT_HISTO_BIN)
		{
			//printf("PFX_LAT_HISTO_BIN data=0x%x\n", *p);
			i++;
			p++;
			sz_rd+=2;
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;
			r2 = *p;
			i++;
			p++;
			sz_rd+=2;
		}
		// Is it a prefix for 9-bit content?
		else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_TIME_BIN_IX)
		{
			i++;
			p++;
			sz_rd+=2;
			si = 0;
			//printf("PFX_TIME_BIN_IX\n");
		}
		else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_HISTO_BIN_IX)
		{
			i++;
			p++;
			sz_rd+=2;
			//printf("PFX_HISTO_BIN_IX\n");
		}
		else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME)
		{
			i++;
			p++;
			sz_rd+=2;
			i++;
			p++;
			sz_rd+=2;
			//printf("PFX_START_OF_DFRAME\n");
		}
		else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_MFRAME)
		{
			i++;
			p++;
			sz_rd+=2;
			i++;
			p++;
			sz_rd+=2;
			//printf("PFX_START_OF_MFRAME\n");
		}
		else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_CFRAME)
		{
			i++;
			p++;
			sz_rd+=2;
			i++;
			p++;
			sz_rd+=2;
			//printf("PFX_START_OF_CFRAME\n");
		}

		// Is it a prefix for 8-bit content?
		else if ((*p & PFX_8_BIT_CONTENT_MASK) == PFX_ASCII_MSG_LEN)
		{
			r0 = GET_ASCII_LEN(*p);
			i++;
			p++;
			sz_rd+=2;
			c = (char *) p;
			for (j=0;j<r0; j++)
			{
				c++;
			}
			// Skip the null string terminating character
			r0++;
			// But if the resulting size is odd, there is another null character that we should skip
			if (r0 & 0x0001)
			{
				r0++;
			}
			p+=(r0>>1);
			i+=(r0>>1);
			sz_rd+=r0;
			//printf("PFX_ASCII_MSG_LEN\n");
		}
		// Is it a prefix for 4-bit content?
		else if ((*p & PFX_4_BIT_CONTENT_MASK) == PFX_START_OF_EVENT)
		{
			i++;
			p++;
			sz_rd+=2;

			// Time Stamp lower 16-bit
			i++;
			p++;
			sz_rd+=2;

			// Time Stamp middle 16-bit
			i++;
			p++;
			sz_rd+=2;

			// Time Stamp upper 16-bit
			i++;
			p++;
			sz_rd+=2;

			// Event Count lower 16-bit
			i++;
			p++;
			sz_rd+=2;

			// Event Count upper 16-bit
			i++;
			p++;
			sz_rd+=2;
			//printf("PFX_START_OF_EVENT\n");
		}
		else if ((*p & PFX_4_BIT_CONTENT_MASK) == PFX_END_OF_EVENT)
		{
			i++;
			p++;
			sz_rd+=2;
			i++;
			p++;
			sz_rd+=2;
			//printf("PFX_END_OF_EVENT\n");
		}
		// Is it a prefix for 2-bit content?

		// Is it a prefix for 0-bit content?
		else if ((*p & PFX_0_BIT_CONTENT_MASK) == PFX_END_OF_FRAME)
		{
			i++;
			p++;
			sz_rd+=2;
			//printf("PFX_END_OF_FRAME\n");
		}
		else if (*p == PFX_NULL_CONTENT)
		{
			i++;
			p++;
			sz_rd+=2;
			//printf("PFX_NULL_CONTENT\n");
		}
		else if (*p == PFX_DEADTIME_HSTAT_BINS)
		{
			p++;
			i++;
			sz_rd+=2;
			p++;
			i++;
			sz_rd+=2;

			ui = (unsigned int *) p;

			ui++;
			i+=2;
			sz_rd+=4;

			ui++;
			i+=2;
			sz_rd+=4;

			ui++;
			i+=2;
			sz_rd+=4;

			ui++;
			i+=2;
			sz_rd+=4;

			ui++;
			i+=2;
			sz_rd+=4;

			ui++;
			i+=2;
			sz_rd+=4;

			ui++;
			i+=2;
			sz_rd+=4;

			ui++;
			i+=2;
			sz_rd+=4;

			ui++;
			i+=2;
			sz_rd+=4;

			// Save last value of pointer
			p = (unsigned short *) ui;
			//printf("PFX_DEADTIME_HSTAT_BINS\n");
		}
		// No interpretable data
		else
		{
			//printf("Non interpretable data 0x%x\n", *p);
			sz_rd+=2;
			p++;
		}

		// Check if end of packet was reached
		if (sz_rd == sz)
		{
			done = 1;
		}
		else if (sz_rd > sz)
		{
			err = -1;
			//printf("Format error: read %d bytes but packet size is %d\n", sz_rd, sz);
			done = 1;
		}
	}
	return (err);
#endif
}

/*******************************************************************************
 Pedestal_ClearHisto 
*******************************************************************************/
int Pedestal_ClearHisto(unsigned short asic, unsigned short channel)
{
#ifdef HAS_PEDESTAL_HISTOGRAMS

	if ( (asic < MAX_NB_OF_ASIC_PER_FEMINOS) && (channel <= ASIC_MAX_CHAN_INDEX) )
	{
		Histo_Clear(&(pedestal_histogram[asic][channel].ped_histo));
		// Mark histogram statistics invalid
		pedestal_histogram[asic][channel].stat_valid = 0;
		return (0);
	}
	else
#endif
	{
		return (ERR_ILLEGAL_PARAMETER);
	}
}							

/*******************************************************************************
 Pedestal_SetHistoOffset
*******************************************************************************/
int Pedestal_SetHistoOffset(unsigned short asic, unsigned short channel, unsigned short offset)
{
#ifdef HAS_PEDESTAL_HISTOGRAMS

	if ( (asic < MAX_NB_OF_ASIC_PER_FEMINOS) && (channel <= ASIC_MAX_CHAN_INDEX) )
	{
		pedestal_histogram[asic][channel].ped_histo.min_bin = offset;
		pedestal_histogram[asic][channel].ped_histo.max_bin = offset + (pedestal_histogram[asic][channel].ped_histo.bin_cnt - 1) * pedestal_histogram[asic][channel].ped_histo.bin_wid;
		// Mark histogram statistics invalid
		pedestal_histogram[asic][channel].stat_valid = 0;
		return (0);
	}
	else
#endif
	{
		return (ERR_ILLEGAL_PARAMETER);
	}
}
