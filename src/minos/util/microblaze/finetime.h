/*******************************************************************************

                           _____________________

 File:        finetime.h

 Description: Definition of a fine grain timer for MicroBlaze

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr

  The timer is supposed to be a 32-bit free running counter incremented at
  a known clock rate (50 MHz in the Feminos)

 History:
  June 2012: created

*******************************************************************************/

#ifndef FINETIME_H
#define FINETIME_H

unsigned int finetime_get_tick(volatile unsigned int* timer);
unsigned int finetime_diff_tick(unsigned int t_start, unsigned int t_stop);

#endif
