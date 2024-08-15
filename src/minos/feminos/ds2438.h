/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        ds2438.h

 Description: Definition of constant/functions for controlling a DS2438.

 Author:      X. de la Broïse,        labroise@cea.fr
              D. Calvet,        calvet@hep.saclay.cea.fr

 History:
  May 2007: created

*******************************************************************************/
#ifndef DS2438_H
#define DS2438_H

#include "ds2438_ps.h"

// Internal functions
int ow_TouchReset(void* dev, int portnum);
int ow_WriteBit(void* dev, int portnum, unsigned char sndbit);
int ow_ReadBit(void* dev, int portnum, unsigned char* rcvbit);

// ROM commands
#define OW_READ_ROM 0x33
#define OW_MATCH_ROM 0x55
#define OW_SKIP_ROM 0xCC
#define OW_SEARCH_ROM 0xF0

// Memory Page 0 Register 0x00
#define OW_P0_R0_ADB 0x40
#define OW_P0_R0_NVB 0x20
#define OW_P0_R0_TB 0x10
#define OW_P0_R0_AD 0x08
#define OW_P0_R0_EE 0x04
#define OW_P0_R0_CA 0x02
#define OW_P0_R0_IAD 0x01

// Memory/Control commands
#define OW_WRITE_SP 0x4E
#define OW_READ_SP 0xBE
#define OW_COPY_SP 0x48
#define OW_CONVERT_T 0x44
#define OW_CONVERT_V 0xB4
#define OW_RECALL_MEM 0xB8

#endif
