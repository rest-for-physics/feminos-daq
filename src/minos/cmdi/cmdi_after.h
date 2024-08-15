/*******************************************************************************

                           _____________________

 File:        cmdi_after.h

 Description: Command interpreter for Feminos card.
  Definition of commands specific to the AFTER chip

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr

 History:
  January 2012: created with code moved from cmdi.c

*******************************************************************************/

#ifndef CMDI_AFTER_H
#define CMDI_AFTER_H

#include "cmdi.h"

/* Function prototypes */
int Cmdi_AfterCommands(CmdiContext* c);

#endif
