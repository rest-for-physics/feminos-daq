/*******************************************************************************

                           _____________________

 File:        blendasic.h

 Description: Header file for a "blend" ASIC register array

 A "blend" ASIC is a structure to hold the copy of the internal registers of
 an ASIC. This structure is define as an array of "blend" registers. All
 blend registers have the same size which is a multiple of 16 bit.
 The blend ASIC provides a common abstraction for the registers of the AFTER
 and AGET chips

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  November 2011: created

*******************************************************************************/
#ifndef BLENDASIC_H
#define BLENDASIC_H

#define BLEND_ASIC_REGISTER_MAX_SIZE_IN_BIT 128
#define BLEND_ASIC_REGISTER_MAX_SIZE_IN_SHORT (BLEND_ASIC_REGISTER_MAX_SIZE_IN_BIT / 16)
#define BLEND_ASIC_REGISTER_MAX_NB_OF_REGISTERS 16

typedef struct _BlendAsicRegister {
    unsigned short bars[BLEND_ASIC_REGISTER_MAX_SIZE_IN_SHORT];
} BlendAsicRegister;
typedef struct _BlendAsic {
    BlendAsicRegister bar[BLEND_ASIC_REGISTER_MAX_NB_OF_REGISTERS];
} BlendAsic;

#endif
