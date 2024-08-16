/*******************************************************************************
                           MINOS
                           _____

 File:        lmk03200.c

 Description: Functions to program the LKM03200 PLL of the TCM

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2012: created from T2K version

  July 2013: tested successfully with LMK03000 - not yet zero-delay LMK03200

  January 2014: added programmable delay to output clocks. Delay value is set
  via Minibios.

*******************************************************************************/

#include "lmk03200.h"

#include <cstdio>

#define MAX_LOCK_WAIT_CNT 100

//
// These settings of dividers make 100 MHz outputs from 100 MHz input
//
#define LMK_R_DIVIDER 4     // 100 MHz input -> 25 MHz at input of Phase Detector
#define LMK_VCO_DIVIDER 6   // 1200 MHz at VCO output -> 200 MHz on Distribution Path
#define LMK_CLKOUT_DIV 1    // 200 MHz on Distribution Path -> 100 MHz on outputs
#define LMK_N_DIVIDER_CAL 8 // 200 MHz on Distribution Path -> 25 MHz at input of Phase Detector during calibration phase
#define LMK_N_DIVIDER_FB 4  // 100 MHz from external feed-back -> 25 MHz at input of Phase Detector in 0-delay mode

static unsigned int lmk03200_register_settings[16] = {
        LMK_R_PUT_DIVIDER((LMK_R_CLK_EN | LMK_R_CLK_MUX_DELDIV), LMK_CLKOUT_DIV), // Register #0 - CLKout0 enabled - division - delay
        LMK_R_PUT_DIVIDER((LMK_R_CLK_EN | LMK_R_CLK_MUX_DELDIV), LMK_CLKOUT_DIV), // Register #1 - CLKout1 enabled - division - delay
        LMK_R_PUT_DIVIDER((LMK_R_CLK_EN | LMK_R_CLK_MUX_DELDIV), LMK_CLKOUT_DIV), // Register #2 - CLKout2 enabled - division - delay
        LMK_R_PUT_DIVIDER((LMK_R_CLK_MUX_BYPASS), LMK_CLKOUT_DIV),                // Register #3 - CLKout3 disabled - bypass - No delay
        LMK_R_PUT_DIVIDER((LMK_R_CLK_MUX_BYPASS), LMK_CLKOUT_DIV),                // Register #4 - CLKout4 disabled - bypass - No delay
        // LMK_R_PUT_DIVIDER((LMK_R_CLK_EN | LMK_R_CLK_MUX_DELDIV), LMK_CLKOUT_DIV), // Register #4 - CLKout4 enabled - division - delay PECL output debug clock
        LMK_R_PUT_DIVIDER((LMK_R_CLK_MUX_BYPASS), LMK_CLKOUT_DIV),                // Register #5 - CLKout5 disabled - bypass - No delay
        LMK_R_PUT_DIVIDER((LMK_R_CLK_EN | LMK_R_CLK_MUX_DELDIV), LMK_CLKOUT_DIV), // Register #6 - CLKout6 enabled (because used for feedback) - division - delay
        LMK_R_PUT_DIVIDER(LMK_R7_VCO_MUX_VCODIV, LMK_CLKOUT_DIV),                 // Register #7 - Distribute VCO divided output
        LMK_R8_DEFAULT,                                                           // Register #8 - value from datasheet
        LMK_R9_DEFAULT,                                                           // Register #9 - value from datasheet Vboost=0
        0,                                                                        // Register #10 - skipped
        (LMK_R11_DEFAULT | LMK_R11_DIV4),                                         // Register #11 - Phase detector > 20 MHz
        0,                                                                        // Register #12 - skipped
        0,                                                                        // Register #13 - computed in the code
        0,                                                                        // Register #14 - computed in the code
        0,                                                                        // Register #15 - computed in the code
};
static unsigned int lmk03200_register_program = 0xEBFF; // Program register 15, 14, 13, 11, 7, 6, 5, 4, 3, 2, 1, 0.

/*******************************************************************************
 UWire_WriteRegister
*******************************************************************************/
int UWire_WriteRegister(Tcm* tcm, unsigned int reg, unsigned int dat) {
    int i;
    unsigned int mask, word_wr, data;
    int err;

    err = 0;
    word_wr = UW_MAKE_COMMAND_WORD(reg, dat);
    mask = 0x80000000;

    // printf("reg %d set to 0x%08x\n", reg, word_wr);

    // Set LE low
    data = 0;
    err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_UW_LE, &data);

    // Set CLK low
    data = 0;
    err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_UW_CLK, &data);

    for (i = 0; i < 32; i++) {
        // Put first bit of data
        if (word_wr & mask) {
            data = R20_UW_DATA;
        } else {
            data = 0;
        }
        err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_UW_DATA, &data);
        mask >>= 1;

        // Set CLK high
        data = R20_UW_CLK;
        err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_UW_CLK, &data);

        // Set CLK low
        data = 0;
        err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_UW_CLK, &data);
    }

    // Set DATA low
    data = 0;
    err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_UW_DATA, &data);

    // Set LE high
    data = R20_UW_LE;
    err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_UW_LE, &data);

    // Set LE low
    data = 0;
    err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_UW_LE, &data);

    return (err);
}

/*******************************************************************************
 UW_ConfigureLMK03200
*******************************************************************************/
int UW_ConfigureLMK03200(Tcm* tcm) {
    int i;
    unsigned int reg, mask;
    int err;
    unsigned int data;
    int retry;
    volatile int waste;

    err = 0;

    // Set GOE low
    data = 0;
    err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_LMK_GOE, &data);

    // Set SYNC_N high
    data = 0;
    err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_LMK_SYNC, &data);

    // Reset device
    err -= UWire_WriteRegister(tcm, 0, LMK_R0_RESET);

    // Compute Register #0
    reg = lmk03200_register_settings[0];
    if (tcm->pll == 2) // For LMK03200
    {
        reg = (reg | LMK_R0_DLD_MODE2 | LMK_R0_FB_MUX_CLK6) & (~LMK_R0_0_DELAY_MODE); // DLD is end of calibration routine, no zero delay mode, feed-back on CLK6
    }
    reg = LMK_R_PUT_DELAY(reg, tcm->pll_odel);
    lmk03200_register_settings[0] = reg;

    // Compute Register #1
    reg = LMK_R_PUT_DELAY(lmk03200_register_settings[1], tcm->pll_odel);
    lmk03200_register_settings[1] = reg;

    // Compute Register #2
    reg = LMK_R_PUT_DELAY(lmk03200_register_settings[2], tcm->pll_odel);
    lmk03200_register_settings[2] = reg;

    // Compute Register #13
    reg = LMK_R13_PUT_OSCIN_FREQ(LMK_R13_DEFAULT, 100);
    reg = LMK_R13_PUT_VCO_R4_LF(reg, 0);
    reg = LMK_R13_PUT_VCO_R3_LF(reg, 0);
    reg = LMK_R13_PUT_VCO_C3C4_LF(reg, 0);
    lmk03200_register_settings[13] = reg;

    // Compute Register #14
    reg = LMK_R14_PUT_PLL_MUX(LMK_R14_EN_CLK_GLOBAL, 3); // Active High Digital Lock Detect
    reg = LMK_R14_PUT_PLL_R(reg, LMK_R_DIVIDER);
    lmk03200_register_settings[14] = reg;

    // Compute Register #15
    reg = LMK_R15_PUT_PLL_CP_GAIN(0, 0);
    reg = LMK_R15_PUT_VCO_DIV(reg, LMK_VCO_DIVIDER);
    reg = LMK_R15_PUT_PLL_N(reg, LMK_N_DIVIDER_CAL);
    lmk03200_register_settings[15] = reg;

    // Download registers
    mask = 0x0001;
    for (i = 0; i < 16; i++) {
        if (lmk03200_register_program & mask) {
            err -= UWire_WriteRegister(tcm, i, lmk03200_register_settings[i]);
        }
        mask <<= 1;
    }

    // Wait until the LD pin is active
    data = 0;
    retry = 0;
    while ((data & R20_LMK_LD) != R20_LMK_LD) {
        // Read the state of LD pin
        if ((err = Tcm_ActOnRegister(tcm, TCM_REG_READ, TCM_UW_PORT_REGISTER, R20_LMK_LD, &data)) < 0) {
            printf("UW_ConfigureLMK03200: Tcm_ActOnRegister failed with error %d\n", err);
            return (err);
        }

        // Introduce a delay
        for (waste = 0; waste < 1000; waste++)
            ;

        // See if too many retry were made
        retry++;
        if (retry > MAX_LOCK_WAIT_CNT) {
            printf("UW_ConfigureLMK03200: PLL Lock Detect pin not active after %d attempts!\n", retry);
            err = -1;
            break;
        }
    }
    // printf("UW_ConfigureLMK03200: PLL lock detected after %d loops\n", retry);

    // Set SYNC_N low
    data = R20_LMK_SYNC;
    err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_LMK_SYNC, &data);

    // Set SYNC_N high
    data = 0;
    err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_LMK_SYNC, &data);

    // Set GOE high
    data = R20_LMK_GOE;
    err -= Tcm_ActOnRegister(tcm, TCM_REG_WRITE, TCM_UW_PORT_REGISTER, R20_LMK_GOE, &data);

    // The LMK3000 or LMK3200 in non zero_delay mode are programmed
    if (tcm->pll == 1) {
        printf("UW_ConfigureLMK03200: PLL set in non zero-delay mode.\n");
    }
    // The LMK3200 can also be put in zero_delay mode
    else if (tcm->pll == 2) {
        // Compute Register #0
        reg = (lmk03200_register_settings[0] | LMK_R0_DLD_MODE2 | LMK_R0_0_DELAY_MODE | LMK_R0_FB_MUX_CLK6);

        // Re-program Register 0 for 0-delay mode
        err -= UWire_WriteRegister(tcm, 0, reg);

        // Compute Register #15
        reg = LMK_R15_PUT_PLL_CP_GAIN(0, 0);
        reg = LMK_R15_PUT_VCO_DIV(reg, LMK_VCO_DIVIDER);
        reg = LMK_R15_PUT_PLL_N(reg, LMK_N_DIVIDER_FB);

        // Re-program Register 15
        err -= UWire_WriteRegister(tcm, 15, reg);

        printf("UW_ConfigureLMK03200: PLL set in zero-delay mode\n");
    } else {
        printf("UW_ConfigureLMK03200: unsupported PLL type %d\n", (int) tcm->pll);
        err = -1;
    }

    return (err);
}
