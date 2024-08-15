/*******************************************************************************

 File:        randgen.c

 Description: This module provides a Random Number Generator.
              See randgen.h for definition.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              I. Mandjavidze, mandjavi@hep.saclay.cea.fr

 History:
    Jan/00  : created
    Dec 2006: re-used without modifications

*******************************************************************************/

#include <math.h>

#include "randgen.h"

/*
 * Definitions
 */
#define RANDOM_CONSTANTE (__int64) 30399561
#define RANDOM_MULTIPLIER (__int64) 1664525
#define RANDOM_SEED 0

/*
 * Global variable
 */
unsigned int Rand_Value = RANDOM_SEED;

void Rand_Seed(int seed) {
    Rand_Value = seed;
}

unsigned int Rand_Raw() {
    unsigned __int64 product;

    product = ((__int64) Rand_Value) * RANDOM_MULTIPLIER + RANDOM_CONSTANTE;
    Rand_Value = (unsigned int) product;

    return (Rand_Value);
}

int Rand_Flat() {
    return (Rand_Raw() & 0x7fffFFFF);
}

int Rand_Uniform(int min, int max) {
    return (((unsigned int) (Rand_Raw() >> 1) % (max - min + 1)) + min);
}

int Rand_Exp(int mean) {
    return ((int) (-(double) mean * log((double) (unsigned int) Rand_Raw() / ((double) 4294967296.))));
}

int Rand_Gauss(int mean, int var) {
    double y1, y2, u1, u2, r, g1;

    do {
        y1 = (double) (unsigned int) Rand_Raw() / ((double) 4294967296.);
        y2 = (double) (unsigned int) Rand_Raw() / ((double) 4294967296.);
        u1 = 2.0 * y1 - 1;
        u2 = 2.0 * y2 - 1;
        r = u1 * u1 + u2 * u2;
    } while (r > 1);
    g1 = mean + sqrt((double) var) * u1 * sqrt(-2. * log(r) / r);
    return ((int) g1);
}

/*****************************************************************************/
