/*******************************************************************************

                           _____________________

 File:        finetime.c

 Description: Implementation of a fine grain timer for MicroBlaze

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr

  The timer is supposed to be a 32-bit free running counter incremented at
  a known clock rate (50 MHz in the Feminos)

 History:
  June 2012: created

*******************************************************************************/

#include "finetime.h"

/*******************************************************************************
 finetime_get_tick
*******************************************************************************/
unsigned int finetime_get_tick(volatile unsigned int* timer) {
    return (*timer);
}

/*******************************************************************************
 finetime_diff_tick
*******************************************************************************/
unsigned int finetime_diff_tick(unsigned int t_start, unsigned int t_stop) {
    if (t_start < t_stop) {
        return (t_stop - t_start);
    } else {
        return (0xFFFFFFFF - t_start + t_stop);
    }
}
