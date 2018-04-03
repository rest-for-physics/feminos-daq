/*******************************************************************************

 File:        programflash.h

 Description: Definitions for functions to reprogram Feminos or TCM SPI
 Flash memory with a new bitstream


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
   October 2012 : created

*******************************************************************************/

#ifndef _PROGRAM_FLASH_H
#define _PROGRAM_FLASH_H

#include "cmdfetcher.h"

int CmdFetcher_ProgramFlash(CmdFetcher *cf);

#endif
