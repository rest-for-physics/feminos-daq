/*******************************************************************************

 File:        randgen.h

 Description: This module provides a Random Number Generator.
              See randgen.c in the OS specific directory for implementation.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              I. Mandjavidze, mandjavi@hep.saclay.cea.fr

 History:
    Jan/00  : created
    Dec 2006: re-used without modifications

*******************************************************************************/

#ifndef RANDGEN_H
#define RANDGEN_H

#include "platform_spec.h"

void Rand_Seed(int seed);
int Rand_Flat();
int Rand_Uniform(int min, int max);
int Rand_Exp(int mean);
int Rand_Gauss(int mean, int var);

#endif /* RAND_GEN */
