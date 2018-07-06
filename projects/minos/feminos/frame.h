/*******************************************************************************
                           
                           _____________________

 File:        frame.h

 Description: Feminos Frame Format


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
   June 2011 : created

   March 2012: changed file name mframe.h to frame.h

   September 2013: defined prefix PFX_SOBE_SIZE 

*******************************************************************************/
#ifndef FRAME_H
#define FRAME_H

//
// Prefix Codes for 14-bit data content
//
#define PFX_14_BIT_CONTENT_MASK    0xC000 // Mask to select 2 MSB's of prefix
#define PFX_CARD_CHIP_CHAN_HIT_IX  0xC000 // Index of Card, Chip and Channel Hit
#define PFX_CARD_CHIP_CHAN_HIT_CNT 0x8000 // Nb of Channel hit for given Card and Chip
#define PFX_CARD_CHIP_CHAN_HISTO   0x4000 // Pedestal Histogram for given Card and Chip

#define PUT_CARD_CHIP_CHAN_HISTO(ca, as, ch) (PFX_CARD_CHIP_CHAN_HISTO | (((ca) & 0x1F) <<9) | (((as) & 0x3) <<7) | (((ch) & 0x7F) <<0))
//
// Prefix Codes for 12-bit data content
//
#define PFX_12_BIT_CONTENT_MASK    0xF000 // Mask to select 4 MSB's of prefix
#define PFX_ADC_SAMPLE             0x3000 // ADC sample
#define PFX_LAT_HISTO_BIN          0x2000 // latency or inter event time histogram bin
#define PFX_CHIP_LAST_CELL_READ    0x1000 // Chip index and last cell read
	
//
// Prefix Codes for 9-bit data content
//
#define PFX_9_BIT_CONTENT_MASK     0xFE00 // Mask to select 7 MSB's of prefix
#define PFX_TIME_BIN_IX            0x0E00 // Time-bin Index
#define PFX_HISTO_BIN_IX           0x0C00 // Histogram bin Index
#define PFX_PEDTHR_LIST            0x0A00 // List of pedestal or thresholds
#define PFX_START_OF_DFRAME        0x0800 // Start of Data Frame + 5 bit source + 4 bit Version
#define PFX_START_OF_MFRAME        0x0600 // Start of Monitoring Frame + 4 bit Version + 5 bit source
#define PFX_START_OF_CFRAME        0x0400 // Start of Configuration Frame + 4 bit Version + 5 bit source
// "0000001" : available for future use

#define PUT_HISTO_BIN_IX(bi)        (PFX_HISTO_BIN_IX | ((bi) & 0x1FF))
#define PUT_PEDTHR_LIST(f, a, m, t) (PFX_PEDTHR_LIST | (((f) & 0x1F)<<4) | (((a) & 0x3)<<2) | (((m) & 0x1)<<1) | (((t) & 0x1)<<0))

//
// Prefix Codes for 8-bit data content
//
#define PFX_8_BIT_CONTENT_MASK      0xFF00 // Mask to select 8 MSB's of prefix
#define PFX_ASCII_MSG_LEN           0x0100 // ASCII message length

//
// Prefix Codes for 4-bit data content
//
#define PFX_4_BIT_CONTENT_MASK      0xFFF0 // Mask to select 12 MSB's of prefix
#define PFX_START_OF_EVENT          0x00F0 // Start of Event + 1 bit free + Event Trigger Type
#define PFX_END_OF_EVENT            0x00E0 // End of Event + 4 MSB of size
//#define 0x00D0 // available for future use
//#define 0x00C0 // available for future use
//#define 0x00B0 // available for future use
// "000000001010" : available for future use
// "000000001001" : available for future use
// "000000001000" : available for future use
 	
//
// Prefix Codes for 2-bit data content
//
#define PFX_2_BIT_CONTENT_MASK      0xFFFC // Mask to select 14 MSB's of prefix
#define PFX_CH_HIT_CNT_HISTO        0x007C // Channel Hit Count Histogram
// "00000000011110" : available for future use	
// "00000000011100" : available for future use	
// "00000000011011" : available for future use	
// "00000000011010" : available for future use	
// "00000000011001" : available for future use	
// "00000000011000" : available for future use	

//
// Prefix Codes for 1-bit data content
//
#define PFX_1_BIT_CONTENT_MASK      0xFFFE // Mask to select 15 MSB's of prefix
// "000000000011111" : available for future use	
// "000000000011110" : available for future use	
// "000000000011101" : available for future use	
// "000000000011100" : available for future use	
// "000000000001111" : available for future use	
// "000000000001110" : available for future use	
// "000000000001101" : available for future use	
// "000000000001100" : available for future use	
// "000000000001011" : available for future use	
// "000000000001010" : available for future use	
// "000000000001001" : available for future use	
// "000000000001000" : available for future use	
	
//
// Prefix Codes for 0-bit data content
//
#define PFX_0_BIT_CONTENT_MASK        0xFFFF // Mask to select 16 MSB's of prefix
#define PFX_END_OF_FRAME              0x000F // End of Frame (any type)
#define PFX_DEADTIME_HSTAT_BINS       0x000E // Deadtime statistics and histogram
#define PFX_PEDESTAL_HSTAT            0x000D // Pedestal histogram statistics
#define PFX_PEDESTAL_H_MD             0x000C // Pedestal histogram Mean and Deviation
#define PFX_SHISTO_BINS               0x000B // Hit S-curve histogram
#define PFX_CMD_STATISTICS            0x000A // Command server statistics
#define PFX_START_OF_BUILT_EVENT      0x0009 // Start of built event
#define PFX_END_OF_BUILT_EVENT        0x0008 // End of built event
#define PFX_EVPERIOD_HSTAT_BINS       0x0007 // Inter Event Time statistics and histogram
#define PFX_SOBE_SIZE                 0x0006 // Start of built event + Size
// "0000000000000101" : available for future use
// "0000000000000100" : available for future use
// "0000000000000011" : available for future use
// "0000000000000010" : available for future use
// "0000000000000001" : available for future use
#define PFX_NULL_CONTENT              0x0000 // Null content

//
// Macros to extract 14-bit data content
//
#define GET_CARD_IX(w)  (((w) & 0x3E00) >>  9)
#define GET_CHIP_IX(w)  (((w) & 0x0180) >>  7)
#define GET_CHAN_IX(w)  (((w) & 0x007F) >>  0)

//
// Macros to extract 12-bit data content
//
#define GET_ADC_DATA(w)              (((w) & 0x0FFF) >>  0)
#define GET_LAT_HISTO_BIN(w)         (((w) & 0x0FFF) >>  0)
#define PUT_LAT_HISTO_BIN(w)         (PFX_LAT_HISTO_BIN | (((w) & 0x0FFF) >>  0))
#define GET_LST_READ_CELL(w)         (((w) & 0x03FF) >>  0)
#define GET_LST_READ_CELL_CHIP_IX(w) (((w) & 0x0C00) >> 10)

//
// Macros to extract 9-bit data content
//
#define GET_TIME_BIN(w)               (((w) & 0x01FF) >>  0)
#define GET_HISTO_BIN(w)              (((w) & 0x01FF) >>  0)
#define GET_PEDTHR_LIST_FEM(w)        (((w) & 0x01F0) >>  4)
#define GET_PEDTHR_LIST_ASIC(w)       (((w) & 0x000C) >>  2)
#define GET_PEDTHR_LIST_MODE(w)       (((w) & 0x0002) >>  1)
#define GET_PEDTHR_LIST_TYPE(w)       (((w) & 0x0001) >>  0)
#define PUT_FVERSION_FEMID(w, fv, id) (((w) & 0xFE00) | (((fv) & 0x0003) <<  7) | (((id) & 0x001F) <<  0))
#define GET_FRAMING_VERSION(w)        (((w) & 0x0180) >>  7)
#define GET_FEMID(w)                  (((w) & 0x001F) >>  0)

//
// Macros to act on 8-bit data content
//
#define GET_ASCII_LEN(w) (((w) & 0x00FF) >>  0)
#define PUT_ASCII_LEN(w) (PFX_ASCII_MSG_LEN | ((w) & 0x00FF))

//
// Macros to act on 4-bit data content
//
#define GET_EVENT_TYPE(w)        (((w) & 0x0007) >>  0)
#define GET_EOE_SIZE(w)          (((w) & 0x000F) >>  0)

//
// Macros to extract 2-bit data content
//
#define GET_CH_HIT_CNT_HISTO_CHIP_IX(w) (((w) & 0x0003) >>  0)
#define PUT_CH_HIT_CNT_HISTO_CHIP_IX(w) (PFX_CH_HIT_CNT_HISTO | ((w) & 0x0003))


#define CURRENT_FRAMING_VERSION 0


// Definition of verboseness flags used by MFrame_Print
#define FRAME_PRINT_ALL              0x00000001
#define FRAME_PRINT_SIZE             0x00000002
#define FRAME_PRINT_HIT_CH           0x00000004
#define FRAME_PRINT_HIT_CNT          0x00000008
#define FRAME_PRINT_CHAN_DATA        0x00000010
#define FRAME_PRINT_HISTO_BINS       0x00000020
#define FRAME_PRINT_ASCII            0x00000040
#define FRAME_PRINT_FRBND            0x00000080
#define FRAME_PRINT_EVBND            0x00000100
#define FRAME_PRINT_NULLW            0x00000200
#define FRAME_PRINT_HISTO_STAT       0x00000400
#define FRAME_PRINT_LISTS            0x00000800
#define FRAME_PRINT_LAST_CELL_READ_0 0x00001000
#define FRAME_PRINT_LAST_CELL_READ_1 0x00002000
#define FRAME_PRINT_LAST_CELL_READ_2 0x00004000
#define FRAME_PRINT_LAST_CELL_READ_3 0x00008000
#define FRAME_PRINT_EBBND            0x00010000

typedef struct {
	unsigned int dataReady;
	unsigned int nSignals;
	unsigned int eventId;
	double timeStamp;
	unsigned int maxSignals;
	unsigned int maxSamples;
	unsigned int bufferSize;
} daqInfo;

void Frame_ToSharedMemory( void *fp, void *fr, int fr_sz, unsigned int vflg, daqInfo *dInfo, unsigned short int *Buffer, int tStart, int tcm );
void Frame_Print(void *fp, void *fr, int fr_sz, unsigned int vflg);
int  Frame_IsCFrame(void *fr, short *err_code);
int  Frame_IsDFrame(void *fr);
int  Frame_IsMsgStat(void *fr);
int  Frame_IsDFrame_EndOfEvent(void *fr);
int  Frame_GetEventTyNbTs(void *fr,
	unsigned short *ev_ty,
	unsigned int   *ev_nb,
	unsigned short *ev_tsl,
	unsigned short *ev_tsm,
	unsigned short *ev_tsh);

#endif

