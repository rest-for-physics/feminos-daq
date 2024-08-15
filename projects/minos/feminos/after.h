/*******************************************************************************

                           _____________________

 File:        after.h

 Description:

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  September 2011 : adapted from T2K code

  July 2013: added TEST_MODE declarations and macros

*******************************************************************************/

#ifndef AFTER_H
#define AFTER_H

#include "feminos.h"

#define MAX_NB_OF_AFTER_PER_FEMINOS 4
#define MAX_CHAN_INDEX_AFTER 78
#define ASIC_REGISTER_MAX_ADDRESS 6

/* Definitions for Register 1 */
#define AFTER_GAIN_MASK 0x0006
#define AFTER_STIME_MASK 0x0078
#define AFTER_TEST_MODE_MASK 0x0180

#define AFTER_GAIN_120FC 0x0000
#define AFTER_GAIN_240FC 0x0001
#define AFTER_GAIN_360FC 0x0002
#define AFTER_GAIN_600FC 0x0003

#define AFTER_PUT_GAIN(w, g) ((w & (~AFTER_GAIN_MASK)) | ((g << 1) & AFTER_GAIN_MASK))
#define AFTER_GET_GAIN(w) ((w & AFTER_GAIN_MASK) >> 1)

#define AFTER_PUT_STIME(w, s) ((w & (~AFTER_STIME_MASK)) | ((s << 3) & AFTER_STIME_MASK))
#define AFTER_GET_STIME(w) ((w & AFTER_STIME_MASK) >> 3)

#define AFTER_PUT_TEST_MODE(w, t) ((w & (~AFTER_TEST_MODE_MASK)) | ((t << 7) & AFTER_TEST_MODE_MASK))
#define AFTER_GET_TEST_MODE(w) ((w & AFTER_TEST_MODE_MASK) >> 7)

/* Definitions for Register 2 */
#define AFTER_RD_FROM_0 0x0008
#define AFTER_TEST_DIGOUT 0x0010
#define AFTER_MARKER 0x0040
#define AFTER_MARK_LVL 0x0080

#define AFTER_PUT_RD_FROM_0(w, b) ((w & (~AFTER_RD_FROM_0)) | ((b << 3) & AFTER_RD_FROM_0))
#define AFTER_GET_RD_FROM_0(w) ((w & AFTER_RD_FROM_0) >> 3)

#define AFTER_PUT_TEST_DIGOUT(w, b) ((w & (~AFTER_TEST_DIGOUT)) | ((b << 4) & AFTER_TEST_DIGOUT))
#define AFTER_GET_TEST_DIGOUT(w) ((w & AFTER_TEST_DIGOUT) >> 4)

#define AFTER_PUT_MARKER(w, m) ((w & (~AFTER_MARKER)) | ((m << 6) & AFTER_MARKER))
#define AFTER_GET_MARKER(w) ((w & AFTER_MARKER) >> 6)

#define AFTER_PUT_MARK_LVL(w, m) ((w & (~AFTER_MARK_LVL)) | ((m << 7) & AFTER_MARK_LVL))
#define AFTER_GET_MARK_LVL(w) ((w & AFTER_MARK_LVL) >> 7)

int After_Write(Feminos* fem, unsigned short asic, unsigned short adr, unsigned short* val);
int After_Read(Feminos* fem, unsigned short asic, unsigned short adr, unsigned short* val);

#endif
