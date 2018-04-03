/*******************************************************************************
                           
                           _____________________

 File:        frame.c

 Description: Feminos Frame Format


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
   June 2011 : created
   
   March 2012: changed file name from mframe.c to frame.c

   September 2013: added code for decoding new prefix PFX_SOBE_SIZE
   Added parameter fr_sz to Frame_Print(). In this function, pointer *fr is now
   interpreted as the first word of the frame instead of the size field as was
   done before. The problem with the previous version is that frame size was
   limited to 64KB because the size was coded with two bytes. This was not an
   issue for printing Ethernet frames, but the same code is used to printout
   full events at the output of the event builder which can be up to 7.5 MB.
   The size has therefore to be coded on a 32-bit integer.

*******************************************************************************/
#include "frame.h"
#include <stdio.h>

/*******************************************************************************
 Frame_IsDFrame_EndOfEvent
*******************************************************************************/
int Frame_IsDFrame_EndOfEvent(void *fr)
{
	unsigned short *s;
	unsigned short sz;

	s = (unsigned short *) fr;
	sz = *s;
	sz = (sz / 2) - 1 - 1 - 1;
	s+=sz;
	
	if ((*s & PFX_4_BIT_CONTENT_MASK) == PFX_END_OF_EVENT)
	{
		return(1);
	}
	else
	{
		return(0);
	}
}

/*******************************************************************************
 Frame_IsCFrame
*******************************************************************************/
int Frame_IsCFrame(void *fr, short *err_code)
{
	unsigned short *s;
	s = (unsigned short *) fr;
	s++;
	
	if ((*s & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_CFRAME)
	{
		s++;
		*err_code = *s;
		return(1);
	}
	else
	{
		return(0);
	}
}

/*******************************************************************************
 Frame_IsDFrame
*******************************************************************************/
int Frame_IsDFrame(void *fr)
{
	unsigned short *s;
	s = (unsigned short *) fr;
	s++;
	
	if ((*s & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME)
	{
		return(1);
	}
	else
	{
		return(0);
	}
}

/*******************************************************************************
 Frame_IsMsgStat
*******************************************************************************/
int Frame_IsMsgStat(void *fr)
{
	unsigned short *s;
	s = (unsigned short *) fr;
	s++;

	if ((*s & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_MFRAME)
	{
		s++; // skip MFRAME header
		s++; // skip size
		if ((*s & PFX_0_BIT_CONTENT_MASK) == PFX_CMD_STATISTICS)
		{
			return(1);
		}
		else
		{
			return(0);
		}
	}
	else
	{
		return(0);
	}
}

/*******************************************************************************
 Frame_GetEventTyNbTs
*******************************************************************************/
int Frame_GetEventTyNbTs(void *fr,
	unsigned short *ev_ty,
	unsigned int   *ev_nb,
	unsigned short *ev_tsl,
	unsigned short *ev_tsm,
	unsigned short *ev_tsh)
{
	unsigned short *p;
	unsigned short r0, r1;
	
	p = (unsigned short *) fr;

	// Is it a Start Of Event?
	if ((*p & PFX_4_BIT_CONTENT_MASK) == PFX_START_OF_EVENT)
	{
		// Get event type
		*ev_ty = GET_EVENT_TYPE(*p);
		p++;

		// Time Stamp lower 16-bit
		*ev_tsl = *p;
		p++;

		// Time Stamp middle 16-bit
		*ev_tsm = *p;
		p++;
		
		// Time Stamp upper 16-bit
		*ev_tsh = *p;
		p++;
	
		// Event Count lower 16-bit
		r0 = *p;
		p++;
		
		// Event Count upper 16-bit
		r1 = *p;
		
		*ev_nb = (((unsigned int) r1) << 16) | ((unsigned int) r0);

		return(0);
	}
	else
	{
		return (-1);
	}
}

/*******************************************************************************
 Frame_Print
*******************************************************************************/
void Frame_Print(void *fp, void *fr, int fr_sz, unsigned int vflg)
{
	unsigned short *p;
	int i, j;
	int sz_rd;
	int done = 0;
	unsigned short r0, r1, r2, r3;
	unsigned int tmp;
	int tmp_i[10];
	int si;
	char *c;
	unsigned int *ui;
	float mean, std_dev;
	char tmp_str[10];

	p = (unsigned short *) fr;

	done  = 0;
	i     = 0;
	sz_rd = 0;
	si    = 0;

	if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_SIZE))
	{
		fprintf((FILE *) fp, "Frame payload: %d bytes\n", fr_sz);
	}

	while (!done)
	{
		// Is it a prefix for 14-bit content?
		if ((*p & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_IX)
		{
			r0 = GET_CARD_IX(*p);
			r1 = GET_CHIP_IX(*p);
			r2 = GET_CHAN_IX(*p);
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HIT_CH))
			{
				fprintf((FILE *) fp, "Card %02d Chip %01d Channel %02d\n", r0, r1, r2);
			}
			i++;
			p++;
			sz_rd+=2;
			si = 0;
		}
		else if ((*p & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_CNT)
		{
			r0 = GET_CARD_IX(*p);
			r1 = GET_CHIP_IX(*p);
			r2 = GET_CHAN_IX(*p);
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HIT_CNT))
			{
				fprintf((FILE *) fp, "Card %02d Chip %01d Channel_Hit_Count %02d\n", r0, r1, r2);
			}
			i++;
			p++;
			sz_rd+=2;
		}
		else if ((*p & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HISTO)
		{
			r0 = GET_CARD_IX(*p);
			r1 = GET_CHIP_IX(*p);
			r2 = GET_CHAN_IX(*p);
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HIT_CH))
			{
				fprintf((FILE *) fp, "Card %02d Chip %01d Channel %02d ", r0, r1, r2);
			}
			i++;
			p++;
			sz_rd+=2;
		}
		// Is it a prefix for 12-bit content?
		else if ((*p & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE)
		{
			r0 = GET_ADC_DATA(*p);
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_CHAN_DATA))
			{
				fprintf((FILE *) fp, "%03d 0x%04x (%4d)\n", si, r0, r0);
			}
			i++;
			p++;
			sz_rd+=2;
			si++;
		}
		else if ((*p & PFX_12_BIT_CONTENT_MASK) == PFX_LAT_HISTO_BIN)
		{
			r0 = GET_LAT_HISTO_BIN(*p);
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
			tmp = (((unsigned int) r2) << 16) | (unsigned int) (r1);
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_BINS))
			{
				fprintf((FILE *) fp, "%03d %03d\n", r0, tmp);
			}
			
		}
		else if ((*p & PFX_12_BIT_CONTENT_MASK) == PFX_CHIP_LAST_CELL_READ)
		{
			r0 = *p;
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
			r3 = *p;
			i++;
			p++;
			sz_rd+=2;
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_LAST_CELL_READ_0))
			{
				fprintf((FILE *) fp, "Chip %01d Last_Cell_Read %03d (0x%03x)\n",
					GET_LST_READ_CELL_CHIP_IX(r0),
					GET_LST_READ_CELL(r0),
					GET_LST_READ_CELL(r0));
			}
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_LAST_CELL_READ_1))
			{
				fprintf((FILE *) fp, "Chip %01d Last_Cell_Read %03d (0x%03x)\n",
					GET_LST_READ_CELL_CHIP_IX(r1),
					GET_LST_READ_CELL(r1),
					GET_LST_READ_CELL(r1));
			}
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_LAST_CELL_READ_2))
			{
				fprintf((FILE *) fp, "Chip %01d Last_Cell_Read %03d (0x%03x)\n",
					GET_LST_READ_CELL_CHIP_IX(r2),
					GET_LST_READ_CELL(r2),
					GET_LST_READ_CELL(r2));
			}
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_LAST_CELL_READ_3))
			{
				fprintf((FILE *) fp, "Chip %01d Last_Cell_Read %03d (0x%03x)\n",
					GET_LST_READ_CELL_CHIP_IX(r3),
					GET_LST_READ_CELL(r3),
					GET_LST_READ_CELL(r3));
			}
		}
		// Is it a prefix for 9-bit content?
		else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_TIME_BIN_IX)
		{
			r0 = GET_TIME_BIN(*p);
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_CHAN_DATA))
			{
				fprintf((FILE *) fp, "Time_Bin: %d\n", r0);
			}
			i++;
			p++;
			sz_rd+=2;
			si = 0;
		}
		else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_HISTO_BIN_IX)
		{
			r0 = GET_HISTO_BIN(*p);
			i++;
			p++;
			sz_rd+=2;
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_BINS))
			{
				fprintf((FILE *) fp, "Bin %3d Val %5d\n", r0, *p);
			}
			i++;
			p++;
			sz_rd+=2;

		}
		else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_PEDTHR_LIST)
		{
			r0 = GET_PEDTHR_LIST_FEM(*p);
			r1 = GET_PEDTHR_LIST_ASIC(*p);
			r2 = GET_PEDTHR_LIST_MODE(*p);
			r3 = GET_PEDTHR_LIST_TYPE(*p);
			i++;
			p++;
			sz_rd+=2;
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_LISTS) )
			{
				if (r3 == 0) // pedestal entry
				{
					fprintf((FILE *) fp, "# Pedestal List for FEM %02d ASIC %01d\n", r0, r1);
				}
				else
				{
					fprintf((FILE *) fp, "# Threshold List for FEM %02d ASIC %01d\n", r0, r1);
				}
				fprintf((FILE *) fp, "fem %02d\n", r0);
			}

			// Determine the number of entries
			if (r2 == 0) // AGET
			{
				r2 = 71;
			}
			else // AFTER
			{
				r2 = 78;
			}
			// Get all entries
			for (j=0;j<=r2; j++)
			{
				tmp_i[0] = (int) * ((short *) p);
				if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_LISTS) )
				{
					if (r3 == 0) // pedestal entry
					{
						sprintf(tmp_str, "ped");
					}
					else
					{
						sprintf(tmp_str, "thr");
					}
					if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_LISTS) )
					{
						fprintf((FILE *) fp, "%s %1d %2d 0x%04x (%4d)\n", tmp_str, r1, j, *p, tmp_i[0]);
					}
				}
				i++;
				p++;
				sz_rd+=2;
			}
		}
		else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME)
		{
			r0 = GET_FRAMING_VERSION(*p);
			r1 = GET_FEMID(*p);
			i++;
			p++;
			sz_rd+=2;
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_FRBND))
			{
				fprintf((FILE *) fp, "--- Start of Data Frame (V.%01d) FEM %02d --\n", r0, r1);
				fprintf((FILE *) fp, "Filled with %d bytes\n", *p);
			}
			i++;
			p++;
			sz_rd+=2;
		}
		else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_MFRAME)
		{
			r0 = GET_FRAMING_VERSION(*p);
			r1 = GET_FEMID(*p);
			i++;
			p++;
			sz_rd+=2;
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_FRBND))
			{
				fprintf((FILE *) fp, "--- Start of Moni Frame (V.%01d) FEM %02d --\n", r0, r1);
				fprintf((FILE *) fp, "Filled with %d bytes\n", *p);
			}
			i++;
			p++;
			sz_rd+=2;
		}
		else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_CFRAME)
		{
			r0 = GET_FRAMING_VERSION(*p);
			r1 = GET_FEMID(*p);
			i++;
			p++;
			sz_rd+=2;
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_FRBND))
			{
				fprintf((FILE *) fp, "--- Start of Config Frame (V.%01d) FEM %02d --\n", r0, r1);
				fprintf((FILE *) fp, "Error code: %d\n", *((short *) p));
			}
			i++;
			p++;
			sz_rd+=2;
		}

		// Is it a prefix for 8-bit content?
		else if ((*p & PFX_8_BIT_CONTENT_MASK) == PFX_ASCII_MSG_LEN)
		{
			r0 = GET_ASCII_LEN(*p);
			i++;
			p++;
			sz_rd+=2;
			c = (char *) p;
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_ASCII))
			{
				for (j=0;j<r0; j++)
				{
					fprintf((FILE *) fp, "%c", *c);
					c++;
				}
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
		}
		// Is it a prefix for 4-bit content?
		else if ((*p & PFX_4_BIT_CONTENT_MASK) == PFX_START_OF_EVENT)
		{
			r0 = GET_EVENT_TYPE(*p);
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_EVBND))
			{
				fprintf((FILE *) fp, "-- Start of Event (Type %01d) --\n", r0);
			}
			i++;
			p++;
			sz_rd+=2;

			// Time Stamp lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;

			// Time Stamp middle 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;

			// Time Stamp upper 16-bit
			r2 = *p;
			i++;
			p++;
			sz_rd+=2;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_EVBND))
			{
				fprintf((FILE *) fp, "Time 0x%04x 0x%04x 0x%04x\n", r2, r1, r0);
			}

			// Event Count lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;

			// Event Count upper 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;

			tmp = (((unsigned int) r1) << 16) | ((unsigned int) r0);
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_EVBND))
			{
				fprintf((FILE *) fp, "Event_Count 0x%08x (%d)\n", tmp, tmp);
			}
		}
		else if ((*p & PFX_4_BIT_CONTENT_MASK) == PFX_END_OF_EVENT)
		{
			tmp = ((unsigned int) GET_EOE_SIZE(*p)) << 16;
			i++;
			p++;
			sz_rd+=2;
			tmp = tmp + (unsigned int) *p;
			i++;
			p++;
			sz_rd+=2;
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_EVBND))
			{
				fprintf((FILE *) fp, "----- End of Event ----- (size %d bytes)\n", tmp);
			}
			
		}

		// Is it a prefix for 2-bit content?
		else if ((*p & PFX_2_BIT_CONTENT_MASK) == PFX_CH_HIT_CNT_HISTO)
		{
			r0 = GET_CH_HIT_CNT_HISTO_CHIP_IX(*p);
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Channel Hit Count Histogram (ASIC %d)\n", r0);
			}

			p++;
			i++;
			sz_rd+=2;
			
			// null word
			p++;
			i++;
			sz_rd+=2;

			ui = (unsigned int *) p;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Min Bin  : %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Max Bin  : %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Bin Width: %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Bin Count: %d\n", *ui);
			}
			r0 = *ui;
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Min Value: %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;
		
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Max Value: %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;
		
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Mean     : %.2f\n", ((float) *ui) / 100.0);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Std Dev  : %.2f\n", ((float) *ui) / 100.0);
			}
			ui++;
			i+=2;
			sz_rd+=4;
		
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Entries  : %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			// Get all bins
			for (j=0; j<r0; j++)
			{
				if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
				{
					fprintf((FILE *) fp, "Bin(%2d) = %9d\n", j, *ui);
				}
				ui++;
				i+=2;
				sz_rd+=4;
			}

			// Save last value of pointer
			p = (unsigned short *) ui;
		}

		// Is it a prefix for 0-bit content?
		else if ((*p & PFX_0_BIT_CONTENT_MASK) == PFX_END_OF_FRAME)
		{
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_FRBND))
			{
				fprintf((FILE *) fp, "----- End of Frame -----\n");
			}
			i++;
			p++;
			sz_rd+=2;
		}
		else if (*p == PFX_NULL_CONTENT)
		{
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_NULLW))
			{
				fprintf((FILE *) fp, "null word (2 bytes)\n");
			}
			i++;
			p++;
			sz_rd+=2;
		}
		else if ((*p == PFX_DEADTIME_HSTAT_BINS) || (*p == PFX_EVPERIOD_HSTAT_BINS))
		{
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				if (*p == PFX_DEADTIME_HSTAT_BINS)
				{
					fprintf((FILE *) fp, "Dead-time Histogram\n");
				}
				else
				{
					fprintf((FILE *) fp, "Inter Event Time Histogram\n");
				}
			}

			p++;
			i++;
			sz_rd+=2;
			
			// null word
			p++;
			i++;
			sz_rd+=2;

			ui = (unsigned int *) p;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Min Bin  : %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Max Bin  : %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Bin Width: %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Bin Count: %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Min Value: %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;
		
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Max Value: %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;
		
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Mean     : %.2f\n", ((float) *ui) / 100.0);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Std Dev  : %.2f\n", ((float) *ui) / 100.0);
			}
			ui++;
			i+=2;
			sz_rd+=4;
		
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Entries  : %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			// Save last value of pointer
			p = (unsigned short *) ui;
		}
		else if (*p == PFX_PEDESTAL_HSTAT)
		{
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "\nPedestal Histogram Statistics\n");
			}
			p++;
			i++;
			sz_rd+=2;

			ui = (unsigned int *) p;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Min Bin  : %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Max Bin  : %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Bin Width: %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Bin Count: %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Min Value: %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Max Value: %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Mean     : %.2f\n", ((float) *ui) / 100.0);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Std Dev  : %.2f\n", ((float) *ui) / 100.0);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Entries  : %d\n", *ui);
			}
			ui++;
			i+=2;
			sz_rd+=4;

			// Save last value of pointer
			p = (unsigned short *) ui;
		}
		else if (*p == PFX_PEDESTAL_H_MD)
		{
			p++;
			i++;
			sz_rd+=2;

			ui = (unsigned int *) p;

			mean = (float) (((float) *ui) / 100.0);
			ui++;
			i+=2;
			sz_rd+=4;

			std_dev = (float) (((float) *ui) / 100.0);
			ui++;
			i+=2;
			sz_rd+=4;

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_STAT))
			{
				fprintf((FILE *) fp, "Mean/Std_dev : %.2f  %.2f\n", mean, std_dev);
			}

			// Save last value of pointer
			p = (unsigned short *) ui;
		}
		else if (*p == PFX_SHISTO_BINS)
		{
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_BINS))
			{
				fprintf((FILE *) fp, "Threshold Turn-on curve\n");
			}
			i++;
			p++;
			sz_rd+=2;

			for (j=0;j<16; j++)
			{
				if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_BINS))
				{
					fprintf((FILE *) fp, "%d ", *p);
				}
				i++;
				p++;
				sz_rd+=2;
			}
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_HISTO_BINS))
			{
				fprintf((FILE *) fp, "\n\n");
			}
		}
		else if (*p == PFX_CMD_STATISTICS)
		{
			// Skip header
			i++;
			p++;
			sz_rd+=2;

			// RX command count lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;
			// RX command count upper 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;
			tmp_i[0] = (int) ((r1 << 16) | (r0));

			// RX daq count lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;
			// RX daq count upper 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;
			tmp_i[1] = (int) ((r1 << 16) | (r0));

			// RX daq timeout lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;
			// RX daq timeout upper 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;
			tmp_i[2] = (int) ((r1 << 16) | (r0));

			// RX daq delayed lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;
			// RX daq delayed upper 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;
			tmp_i[3] = (int) ((r1 << 16) | (r0));

			// Missing daq requests lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;
			// Missing daq requests upper 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;
			tmp_i[4] = (int) ((r1 << 16) | (r0));

			// RX command error count lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;
			// RX command error count upper 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;
			tmp_i[5] = (int) ((r1 << 16) | (r0));

			// TX command reply count lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;
			// TX command reply count upper 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;
			tmp_i[6] = (int) ((r1 << 16) | (r0));

			// TX DAQ reply count lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;
			// TX DAQ reply count upper 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;
			tmp_i[7] = (int) ((r1 << 16) | (r0));

			// TX DAQ reply re-send count lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;
			// TX DAQ reply re-send count upper 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;
			tmp_i[8] = (int) ((r1 << 16) | (r0));

			if (vflg & FRAME_PRINT_ALL)
			{
				fprintf((FILE *) fp, "Server RX stat: cmd_count=%d daq_req=%d daq_timeout=%d daq_delayed=%d daq_missing=%d cmd_errors=%d\n", tmp_i[0], tmp_i[1], tmp_i[2], tmp_i[3], tmp_i[4], tmp_i[5]);
				fprintf((FILE *) fp, "Server TX stat: cmd_replies=%d daq_replies=%d daq_replies_resent=%d\n", tmp_i[6], tmp_i[7], tmp_i[8]);
			}
		}
		else if (*p == PFX_START_OF_BUILT_EVENT)
		{
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_EBBND))
			{
				fprintf((FILE *) fp, "***** Start of Built Event *****\n");
			}
			i++;
			p++;
			sz_rd+=2;
		}
		else if (*p == PFX_END_OF_BUILT_EVENT)
		{
			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_EBBND))
			{
				fprintf((FILE *) fp, "***** End of Built Event *****\n\n");
			}
			i++;
			p++;
			sz_rd+=2;
		}
		else if (*p == PFX_SOBE_SIZE)
		{
			// Skip header
			i++;
			p++;
			sz_rd+=2;

			// Built Event Size lower 16-bit
			r0 = *p;
			i++;
			p++;
			sz_rd+=2;
			// Built Event Size upper 16-bit
			r1 = *p;
			i++;
			p++;
			sz_rd+=2;
			tmp_i[0] = (int) ((r1 << 16) | (r0));

			if ( (vflg & FRAME_PRINT_ALL) || (vflg & FRAME_PRINT_EBBND))
			{
				fprintf((FILE *) fp, "***** Start of Built Event - Size = %d bytes *****\n", tmp_i[0]);
			}

		}
		// No interpretable data
		else
		{
			fprintf((FILE *) fp, "word(%04d) : 0x%x (%d) unknown data\n", i, *p, *p);
			sz_rd+=2;
			p++;
		}
		
		// Check if end of packet was reached
		if (sz_rd == fr_sz)
		{
			done = 1;
		}
		else if (sz_rd > fr_sz)
		{
			fprintf((FILE *) fp, "Format error: read %d bytes but packet size is %d\n", sz_rd, fr_sz);
			done = 1;
		}
	}

	// Put an empty line
	if (vflg & FRAME_PRINT_ALL)
	{
		fprintf((FILE *) fp, "\n");
	}
}

