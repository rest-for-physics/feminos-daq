/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        ds2438_api.h

 Description: Definition of user API for controlling a DS2438.

 Author:      X. de la Bro�se,        labroise@cea.fr
              D. Calvet,        calvet@hep.saclay.cea.fr

 History:
  October 2007: created

*******************************************************************************/
#ifndef DS2438_API_H
#define DS2438_API_H

// Command codes
#define OW_CONV_TEMP 0x01
#define OW_CONV_VDD 0x02
#define OW_CONV_VAD 0x03
#define OW_CONV_I 0x04

// User API functions
int ow_GetSerialNum(void* dev, int portnum, unsigned char* SNum);
int ow_GetConversion(void* dev, int op, int portnum, short* cv);

// OW error codes
#define OWERROR_DEVICE_BUSY -200
#define OWERROR_PRESENCE_NOT_FOUND -201
#define OWERROR_INCORRECT_CRC -202
#define OWERROR_CONVERSION_TIMEOUT -203
#define OWERROR_ILLEGAL_CONVERSION -204

#endif
