/**************************************************************
 *  Project          : Mars EDK reference design
 *  File description : I2C suppot for Mars FPGA modules
 *  File name        : MarsI2C.h
 *  Author           : Christoph Glattfelder
 **************************************************************
 *  Copyright (c) 2011 by Enclustra GmbH, Switzerland
 *  All rights reserved.
 **************************************************************
 *  Notes:
 *
 **************************************************************
 *  File history:
 *
 *  Version | Date     | Author             | Remarks
 *  -----------------------------------------------------------
 *  1.0	    | 18.03.11 | Ch. Glattfelder    | Created
 *          |          |                    |
 *  ------------------------------------------------------------
 *
 **************************************************************/

#ifndef MARSI2C_H_
#define MARSI2C_H_

/*
 * The following constant defines the address of the IIC
 * device on the IIC bus. Note that since the address is only 7 bits, this
 * constant is the address divided by 2.
 */

#define CURRSENS_MARS 		0x40	/* Current Monitor on Mars - 7 bit number */
#define CURRSENS_STARTER 	0x4F	/* Current Monitor on Mars Starter - 7 bit number */
#define GPIO_ADDR 			0x21	/* GPIO - 7 bit number */
#define RTC_ADDR 			0x6F	/* RTC - 7 bit number */
#define EEPROM_ADDR 		0x50	/* EEPROM - 7 bit number */


extern volatile u8 TransmitComplete;
extern volatile u8 ReceiveComplete;
extern XIic IicInstance;

void SendHandler(XIic * InstancePtr);
void ReceiveHandler(XIic * InstancePtr);
void StatusHandler(XIic *InstancePtr, int Event);


void CurrSenseInit(int DevAddr);
int CurrSenseWriteConfig(u16 Value);
int CurrSenseWriteCalib(u16 Value);
int CurrSenseReadConfig(u16* pValue);
int CurrSenseReadVShunt(u16* pValue);
int CurrSenseReadVBus(u16* pValue);
int CurrSenseReadPower(u16* pValue);
int CurrSenseReadCurrent(u16* pValue);
int CurrSenseReadCalib(u16* pValue);

int GpioSetDir(u8 Value);
int GpioOut(u8 Value);
int GpioSetLed(u8 Value);

int RTC_init();
int RTC_readTime(int* hour, int* min, int* sec);
int RTC_setTime(int hour, int min, int sec);
int RTC_readDate(int* day, int* month, int* year);
int RTC_setDate(int day, int month, int year);
int RTC_readTemp(int* temp);

int EEPROM_init(int DevAddr);
int EEPROM_read(u32 addr, u32 len, u8* readbuffer);

#endif /* MARSI2C_H_ */
