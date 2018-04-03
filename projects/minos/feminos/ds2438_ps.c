/*******************************************************************************
                              MINOS - Feminos
                           _____________________

 File:        ds2438_ps.c

 Description: Implementation of functions for controlling a DS2438.
  Platform dependent implementation: Micro-Blaze version

 Author:      X. de la Bro�se,        labroise@cea.fr
              D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  May 2007: created
  September 2012: adapted to Feminos from T2K Fem version

*******************************************************************************/

#include "ds2438.h"
#include "ds2438_api.h"
#include "feminos.h"
#include "cmdi.h"

//--------------------------------------------------------------------------
// Reset all of the devices on the 1-Wire Net and return the result.
//
// dev      a pointer to the device
// portnum  this number is provided to indicate the symbolic port number.
//
// Return :   0 : presence pulse(s) detected, device(s) reset
//          < 0 : no presence pulses detected
//
int ow_TouchReset(void *dev, int portnum)
{
	unsigned int val_wr, val_rd;
	int retry;
	int err;
	CmdiContext *c = (CmdiContext *) dev;

	// Make RESET pulse
	val_wr = SI_ID_CHIPS_RESET | SI_ID_CHIPS_LDACT;
	if ( (err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, ADDR_SI_ID_CHIPS, MSK_SI_ID_CHIPS, &val_wr)) < 0)
	{
		return (err);
	}

	// Clear LDACT bit but keep the command
	val_wr &= ~SI_ID_CHIPS_LDACT;
	if ( (err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, ADDR_SI_ID_CHIPS, MSK_SI_ID_CHIPS, &val_wr)) < 0)
	{
		return (err);
	}

	// Wait until Busy flag cleared
	retry = 0;
	do
	{
		if ( (err = Feminos_ActOnRegister(c->fem, FM_REG_READ, ADDR_SI_ID_CHIPS, MSK_SI_ID_CHIPS, &val_rd)) < 0)
		{
			return (err);
		}
		if (retry > OW_MAX_RETRY)
		{
			return (OWERROR_DEVICE_BUSY);
		}
		retry++;
	} while (val_rd & SI_ID_CHIPS_BUSY);

	// Check Presence pulse
	if ((val_rd & SI_ID_CHIPS_MISO) == SI_ID_CHIPS_MISO)
	{
		return (OWERROR_PRESENCE_NOT_FOUND);
	}
	else
	{
		return (0);
	}
}

//--------------------------------------------------------------------------
// Send a 1-Wire write bit.
//
// dev      a pointer to the device
// portnum  this number is provided to indicate the symbolic port number.
// sndbit   the least significant bit is the bit to send
//
// Return :   0 : success
//          < 0 : failed
//
int ow_WriteBit(void *dev, int portnum, unsigned char sndbit)
{
	unsigned int val_wr, val_rd;
	int err;
	int retry;
	CmdiContext *c = (CmdiContext *) dev;

	// Prepare then write bit of data
	if (sndbit & 0x1)
	{
		val_wr = SI_ID_CHIPS_WRITE_1 | SI_ID_CHIPS_LDACT;
	}
	else
	{
		val_wr = SI_ID_CHIPS_WRITE_0 | SI_ID_CHIPS_LDACT;
	}
	if ( (err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, ADDR_SI_ID_CHIPS, MSK_SI_ID_CHIPS, &val_wr)) < 0)
	{
		return (err);
	}

	// Clear LDACT bit but keep the command
	val_wr &= ~SI_ID_CHIPS_LDACT;
	if ( (err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, ADDR_SI_ID_CHIPS, MSK_SI_ID_CHIPS, &val_wr)) < 0)
	{
		return (err);
	}

	// Wait until Busy flag cleared
	retry = 0;
	do
	{
		if ( (err = Feminos_ActOnRegister(c->fem, FM_REG_READ, ADDR_SI_ID_CHIPS, MSK_SI_ID_CHIPS, &val_rd)) < 0)
		{
			return (err);
		}
		if (retry > OW_MAX_RETRY)
		{
			return (OWERROR_DEVICE_BUSY);
		}
		retry++;
	} while (val_rd & SI_ID_CHIPS_BUSY);

	return (0);
}

//--------------------------------------------------------------------------
// Send a 1-Wire read bit.
//
// dev      a pointer to the device
// portnum  this number is provided to indicate the symbolic port number.
// rcvbit   pointer to receive the bit of data read
// Return :   0 : success
//          < 0 : failed
//
int ow_ReadBit(void *dev, int portnum, unsigned char *rcvbit)
{
	unsigned int val_wr, val_rd;
	int err;
	int retry;
	CmdiContext *c = (CmdiContext *) dev;
	
	// Send a read bit command
	val_wr = SI_ID_CHIPS_READ_BIT | SI_ID_CHIPS_LDACT;
	if ( (err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, ADDR_SI_ID_CHIPS, MSK_SI_ID_CHIPS, &val_wr)) < 0)
	{
		return (err);
	}

	// Clear LDACT bit but keep the command
	val_wr &= ~SI_ID_CHIPS_LDACT;
	if ( (err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, ADDR_SI_ID_CHIPS, MSK_SI_ID_CHIPS, &val_wr)) < 0)
	{
		return (err);
	}

	// Wait until Busy flag cleared
	retry = 0;
	do
	{
		if ( (err = Feminos_ActOnRegister(c->fem, FM_REG_READ, ADDR_SI_ID_CHIPS, MSK_SI_ID_CHIPS, &val_rd)) < 0)
		{
			return (err);
		}
		if (retry > OW_MAX_RETRY)
		{
			return (OWERROR_DEVICE_BUSY);
		}
		retry++;
	} while (val_rd & SI_ID_CHIPS_BUSY);

	// Extract bit of data
   if ((val_rd & SI_ID_CHIPS_MISO) == SI_ID_CHIPS_MISO)
	{
		*rcvbit = 0x1;
	}
	else
	{
		*rcvbit = 0x0;
	}

	return (0);
}
