/*******************************************************************************

                           _____________________

 File:        cmdi_i2c.h

 Description: Definition of command interpreter functions to control I2C devices
   on an Enclustra Mars MX2 module.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr

 History:

  October 2012: created by extraction from cmdi.c

*******************************************************************************/

#ifndef _CMDI_I2C_H
#define _CMDI_I2C_H

#ifdef TARGET_FEMINOS
#include "cmdi.h"
#endif
#ifdef TARGET_TCM
#include "tcm_cmdi.h"
#endif

int Cmdi_I2CCommands(CmdiContext* c);

#endif
