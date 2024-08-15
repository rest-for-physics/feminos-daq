/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        i2c.h

 Description: API for controlling I2C devices of Enclustra Mars MX2 Module.

 Adapted from sample code provided by Enclustra

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  September 2012: created

*******************************************************************************/

#ifndef I2C_H

#define I2C_H

#include "xiic.h"

int I2C_Open(XIic* i2c_inst, XIntc* x_intc);
int I2C_GetModuleInfo(XIic* i2c_inst, char* str);
int I2C_GetCurrentMonitor(XIic* i2c_inst, char* str);
int I2C_GetDateTimeTemp(XIic* i2c_inst, char* str);
int I2C_SetDateTime(XIic* i2c_inst, char* str, int day, int month, int year, int hour, int min, int sec);

#endif
