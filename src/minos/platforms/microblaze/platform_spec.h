/*******************************************************************************

                           _____________________

 File:        platform_spec.h

 Description: Specific include file and definitions for MicroBlaze standalone
 in Spartan 6

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  May 2011: created

*******************************************************************************/
#ifndef PLATFORM_SPEC_H
#define PLATFORM_SPEC_H

#include "xil_cache.h"
#include "xparameters.h"

#define __int64 long long

#define yield() ;

#define htons(x) ((((x) >> 8) & 0x00FF) | (((x) & 0xFF) << 8))
#define ntohs(x) ((((x) >> 8) & 0x00FF) | (((x) & 0xFF) << 8))
#define htonl(x) ((x))
#define ntohl(x) ((x))

#endif
