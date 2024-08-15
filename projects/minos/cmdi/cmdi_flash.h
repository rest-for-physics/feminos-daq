/*******************************************************************************
                           Minos / Feminos
                           _______________

 File:        cmdi_flash.h

 Description: Definition of command interpreter function to read/write SPI Flash

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr

 History:

  October 2012: created

*******************************************************************************/

#ifndef _CMDI_FLASH_H
#define _CMDI_FLASH_H

#ifdef TARGET_FEMINOS
#include "cmdi.h"
#endif
#ifdef TARGET_TCM
#include "tcm_cmdi.h"
#endif

int Cmdi_FlashCommands(CmdiContext* c);

#endif
