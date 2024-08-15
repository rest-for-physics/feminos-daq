/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        interrupt.h

 Description: Interrupt System.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  September 2012: created

*******************************************************************************/

#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "xintc.h"

extern XIntc InterruptSystem;

int InterruptSystem_Setup();

#endif
