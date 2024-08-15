/*******************************************************************************
                           Minos
                           _____

 File:        lmk3200.h

 Description: Definition of the interface to program the LMK3200 PLL
  on-board the TCM board

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2012: created

*******************************************************************************/

#ifndef LMK03200_H
#define LMK03200_H

#include "tcm.h"

// Defines which TCM register is used for the micro-Wire port
#define TCM_UW_PORT_REGISTER 20

// Constants for Register #0
#define LMK_R0_RESET 0x80000000
#define LMK_R0_DLD_MODE2 0x10000000
#define LMK_R0_0_DELAY_MODE 0x08000000
#define LMK_R0_FB_MUX_CLK5 0x00000000
#define LMK_R0_FB_MUX_FBCLK 0x02000000
#define LMK_R0_FB_MUX_CLK6 0x04000000

// Constants that are common to Register #0 to #7
#define LMK_R_CLK_EN 0x00010000
#define LMK_R_CLK_MUX_BYPASS 0x00000000
#define LMK_R_CLK_MUX_DIVIDED 0x00020000
#define LMK_R_CLK_MUX_DELAYED 0x00040000
#define LMK_R_CLK_MUX_DELDIV 0x00060000

// Constants for Register #7
#define LMK_R7_VCO_MUX_VCODIV 0x00000000
#define LMK_R7_VCO_MUX_VCO 0x04000000

// Constants for Register #8
#define LMK_R8_DEFAULT 0x10000908

// Constants for Register #9
#define LMK_R9_DEFAULT 0xA0022A09
#define LMK_R9_VBOOST 0x00010000

// Constants for Register #11
#define LMK_R11_DEFAULT 0x00820000
#define LMK_R11_DIV4 0x00008000

// Constants for Register #13
#define LMK_R13_DEFAULT 0x02800000

// Constants for Register #14
#define LMK_R14_EN_FOUT 0x10000000
#define LMK_R14_EN_CLK_GLOBAL 0x08000000
#define LMK_R14_POWERDOWN 0x04000000

// Macros
#define UW_MAKE_COMMAND_WORD(r, d) (((r) & 0xF) | ((d) & 0xFFFFFFF0))

#define LMK_R_PUT_DELAY(r, d) (((r) & 0xFFFFFF0F) | (((d) & 0x0000000F) << 4))
#define LMK_R_PUT_DIVIDER(r, d) (((r) & 0xFFFF00FF) | (((d) & 0x000000FF) << 8))

#define LMK_R13_PUT_OSCIN_FREQ(r, f) (((r) & 0xFFC03FFF) | (((f) & 0x000FF) << 14))
#define LMK_R13_PUT_VCO_R4_LF(r, f) (((r) & 0xFFFFC7FF) | (((f) & 0x00007) << 11))
#define LMK_R13_PUT_VCO_R3_LF(r, f) (((r) & 0xFFFFF8FF) | (((f) & 0x00007) << 8))
#define LMK_R13_PUT_VCO_C3C4_LF(r, f) (((r) & 0xFFFFFF0F) | (((f) & 0x0000F) << 4))

#define LMK_R14_PUT_PLL_MUX(r, f) (((r) & 0xFF0FFFFF) | (((f) & 0x0000F) << 20))
#define LMK_R14_PUT_PLL_R(r, f) (((r) & 0xFFF000FF) | (((f) & 0x00FFF) << 8))
#define LMK_R14_PUT_PLL_R_DLY(r, f) (((r) & 0xFFFFFF0F) | (((f) & 0x0000F) << 4))

#define LMK_R15_PUT_PLL_CP_GAIN(r, f) (((r) & 0x3FFFFFFF) | (((f) & 0x00003) << 30))
#define LMK_R15_PUT_VCO_DIV(r, f) (((r) & 0xC3FFFFFF) | (((f) & 0x0000F) << 26))
#define LMK_R15_PUT_PLL_N(r, f) (((r) & 0xFC0000FF) | (((f) & 0x3FFFF) << 8))
#define LMK_R15_PUT_PLL_N_DLY(r, f) (((r) & 0xFFFFFF0F) | (((f) & 0x0000F) << 4))

// Function prototypes
int UWire_WriteRegister(Tcm* tcm, unsigned int reg, unsigned int dat);
int UW_ConfigureLMK03200(Tcm* tcm);

#endif
