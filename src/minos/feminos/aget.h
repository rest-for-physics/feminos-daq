/*******************************************************************************

                           _____________________

 File:        aget.h

 Description: Definition of the API to control AGET chip with Feminos readout
 electronics.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  September 2011: adapted from T2K version

*******************************************************************************/
#ifndef AGET_H
#define AGET_H

#include "feminos.h"

#define MAX_NB_OF_AGET_PER_FEMINOS 4
#define MAX_CHAN_INDEX_AGET 71
#define MAX_AGET_REAL_CHAN_CNT 64
#define AGET_REGISTER_MAX_ADDRESS 12

#define CHANNEL_ACCESS_MODE 0
#define SLOWCTRL_ACCESS_MODE 1

// AGET Register definition and bit field manipulation macros (Registers are considered 16-bit wide)
// Register #1
#define AGET_R1S0_ICSA 0x0001
#define AGET_R1S0_TIME_MASK 0x0078
#define AGET_R1S0_TEST_MASK 0x0180
#define AGET_R1S0_MODE_MASK 0x4000
#define AGET_R1S0_FPN_MASK 0x8000
#define AGET_R1S1_POLARITY 0x0001
#define AGET_R1S1_VICM_MASK 0x0006
#define AGET_R1S1_DAC_MASK 0x0078
#define AGET_R1S1_TVETO_MASK 0x0180
#define AGET_R1S1_SDISCRI 0x0200
#define AGET_R1S1_TOT 0x0400
#define AGET_R1S1_RANGETW 0x0800
#define AGET_R1S1_TRIGW_MASK 0x3000

#define AGET_R1S0_PUT_ICSA(r1, ic) ((r1 & (~AGET_R1S0_ICSA)) | (ic & AGET_R1S0_ICSA))
#define AGET_R1S0_PUT_TIME(r1, ti) ((r1 & (~AGET_R1S0_TIME_MASK)) | ((ti << 3) & AGET_R1S0_TIME_MASK))
#define AGET_R1S0_PUT_TEST(r1, te) ((r1 & (~AGET_R1S0_TEST_MASK)) | ((te << 7) & AGET_R1S0_TEST_MASK))
#define AGET_R1S0_PUT_MODE(r1, mo) ((r1 & (~AGET_R1S0_MODE_MASK)) | ((mo << 14) & AGET_R1S0_MODE_MASK))
#define AGET_R1S0_PUT_FPN(r1, fp) ((r1 & (~AGET_R1S0_FPN_MASK)) | ((fp << 15) & AGET_R1S0_FPN_MASK))
#define AGET_R1S1_PUT_POLARITY(r1, po) ((r1 & (~AGET_R1S1_POLARITY)) | ((po << 0) & AGET_R1S1_POLARITY))
#define AGET_R1S1_PUT_VICM(r1, vi) ((r1 & (~AGET_R1S1_VICM_MASK)) | ((vi << 1) & AGET_R1S1_VICM_MASK))
#define AGET_R1S0_GET_ICSA(r1) (r1 & AGET_R1S0_ICSA)
#define AGET_R1S0_GET_TIME(r1) ((r1 & AGET_R1S0_TIME_MASK) >> 3)
#define AGET_R1S0_GET_TEST(r1) ((r1 & AGET_R1S0_TEST_MASK) >> 7)
#define AGET_R1S0_GET_MODE(r1) ((r1 & AGET_R1S0_MODE_MASK) >> 14)
#define AGET_R1S0_GET_FPN(r1) ((r1 & AGET_R1S0_FPN_MASK) >> 15)
#define AGET_R1S1_GET_POLARITY(r1) ((r1 & AGET_R1S1_POLARITY) >> 0)
#define AGET_R1S1_GET_VICM(r1) ((r1 & AGET_R1S1_VICM_MASK) >> 1)
#define AGET_R1S1_PUT_DAC(r1, da) ((r1 & (~AGET_R1S1_DAC_MASK)) | ((da << 3) & AGET_R1S1_DAC_MASK))
#define AGET_R1S1_GET_DAC(r1) ((r1 & AGET_R1S1_DAC_MASK) >> 3)
#define AGET_R1S1_PUT_TVETO(r1, ve) ((r1 & (~AGET_R1S1_TVETO_MASK)) | ((ve << 7) & AGET_R1S1_TVETO_MASK))
#define AGET_R1S1_GET_TVETO(r1) ((r1 & AGET_R1S1_TVETO_MASK) >> 7)
#define AGET_R1S1_PUT_SDISCRI(r1, sd) ((r1 & (~AGET_R1S1_SDISCRI)) | ((sd << 9) & AGET_R1S1_SDISCRI))
#define AGET_R1S1_GET_SDISCRI(r1) ((r1 & AGET_R1S1_SDISCRI) >> 9)
#define AGET_R1S1_PUT_TOT(r1, tot) ((r1 & (~AGET_R1S1_TOT)) | ((tot << 10) & AGET_R1S1_TOT))
#define AGET_R1S1_GET_TOT(r1) ((r1 & AGET_R1S1_TOT) >> 10)
#define AGET_R1S1_PUT_RANGETW(r1, tw) ((r1 & (~AGET_R1S1_RANGETW)) | ((tw << 11) & AGET_R1S1_RANGETW))
#define AGET_R1S1_GET_RANGETW(r1) ((r1 & AGET_R1S1_RANGETW) >> 11)
#define AGET_R1S1_PUT_TRIGW(r1, tw) ((r1 & (~AGET_R1S1_TRIGW_MASK)) | ((tw << 12) & AGET_R1S1_TRIGW_MASK))
#define AGET_R1S1_GET_TRIGW(r1) ((r1 & AGET_R1S1_TRIGW_MASK) >> 12)

// Register #2
#define AGET_R2S0_READ_FROM_0 0x0008
#define AGET_R2S0_TEST_DIGOUT 0x0010
#define AGET_R2S0_EN_MKR_RST 0x0040
#define AGET_R2S0_RST_LEVEL 0x0080
#define AGET_R2S0_CUR_RA_M 0x3000
#define AGET_R2S0_CUR_BUF_M 0xC000
#define AGET_R2S1_SHORT_READ 0x0008
#define AGET_R2S1_DIS_MULTIPLICITY_OUT 0x0010
#define AGET_R2S1_AUTORESET_BANK 0x0020
#define AGET_R2S1_IN_DYN_RANGE 0x0100

#define AGET_R2S0_PUT_READ_FROM_0(r2, rz) ((r2 & (~AGET_R2S0_READ_FROM_0)) | ((rz << 3) & AGET_R2S0_READ_FROM_0))
#define AGET_R2S0_GET_READ_FROM_0(r2) ((r2 & AGET_R2S0_READ_FROM_0) >> 3)
#define AGET_R2S0_PUT_TEST_DIGOUT(r2, dg) ((r2 & (~AGET_R2S0_TEST_DIGOUT)) | ((dg << 4) & AGET_R2S0_TEST_DIGOUT))
#define AGET_R2S0_GET_TEST_DIGOUT(r2) ((r2 & AGET_R2S0_TEST_DIGOUT) >> 4)
#define AGET_R2S0_PUT_EN_MKR_RST(r2, mk) ((r2 & (~AGET_R2S0_EN_MKR_RST)) | ((mk << 6) & AGET_R2S0_EN_MKR_RST))
#define AGET_R2S0_GET_EN_MKR_RST(r2) ((r2 & AGET_R2S0_EN_MKR_RST) >> 6)
#define AGET_R2S0_PUT_RST_LEVEL(r2, rs) ((r2 & (~AGET_R2S0_RST_LEVEL)) | ((rs << 7) & AGET_R2S0_RST_LEVEL))
#define AGET_R2S0_GET_RST_LEVEL(r2) ((r2 & AGET_R2S0_RST_LEVEL) >> 7)
#define AGET_R2S0_PUT_CUR_RA(r2, cr) ((r2 & (~AGET_R2S0_CUR_RA_M)) | ((cr << 12) & AGET_R2S0_CUR_RA_M))
#define AGET_R2S0_GET_CUR_RA(r2) ((r2 & AGET_R2S0_CUR_RA_M) >> 12)
#define AGET_R2S0_PUT_CUR_BUF(r2, cb) ((r2 & (~AGET_R2S0_CUR_BUF_M)) | ((cb << 14) & AGET_R2S0_CUR_BUF_M))
#define AGET_R2S0_GET_CUR_BUF(r2) ((r2 & AGET_R2S0_CUR_BUF_M) >> 14)
#define AGET_R2S1_PUT_SHORT_READ(r2, sr) ((r2 & (~AGET_R2S1_SHORT_READ)) | ((sr << 3) & AGET_R2S1_SHORT_READ))
#define AGET_R2S1_GET_SHORT_READ(r2) ((r2 & AGET_R2S1_SHORT_READ) >> 3)
#define AGET_R2S1_PUT_DIS_MULTIPLICITY_OUT(r2, mo) ((r2 & (~AGET_R2S1_DIS_MULTIPLICITY_OUT)) | ((mo << 4) & AGET_R2S1_DIS_MULTIPLICITY_OUT))
#define AGET_R2S1_GET_DIS_MULTIPLICITY_OUT(r2) ((r2 & AGET_R2S1_DIS_MULTIPLICITY_OUT) >> 4)
#define AGET_R2S1_PUT_AUTORESET_BANK(r2, ab) ((r2 & (~AGET_R2S1_AUTORESET_BANK)) | ((ab << 5) & AGET_R2S1_AUTORESET_BANK))
#define AGET_R2S1_GET_AUTORESET_BANK(r2) ((r2 & AGET_R2S1_AUTORESET_BANK) >> 5)
#define AGET_R2S1_PUT_IN_DYN_RANGE(r2, ir) ((r2 & (~AGET_R2S1_IN_DYN_RANGE)) | ((ir << 8) & AGET_R2S1_IN_DYN_RANGE))
#define AGET_R2S1_GET_IN_DYN_RANGE(r2) ((r2 & AGET_R2S1_IN_DYN_RANGE) >> 8)

// Register #6 and #7
#define AGET_R67_GAIN_MASK 0x0003

#define AGET_R67_PUT_GAIN(r67, shi, gan) ((r67 & (~(AGET_R67_GAIN_MASK << shi))) | ((gan & AGET_R67_GAIN_MASK) << shi))
#define AGET_R67_GET_GAIN(r67, shi) ((r67 & (AGET_R67_GAIN_MASK << shi)) >> shi)

// Register #8 and #9
#define AGET_R89_THRESHOLD_MASK 0x000F

#define AGET_R89_PUT_THRESHOLD(r89, shi, thr) ((r89 & (~(AGET_R89_THRESHOLD_MASK << shi))) | ((thr & AGET_R89_THRESHOLD_MASK) << shi))
#define AGET_R89_GET_THRESHOLD(r89, shi) ((r89 & (AGET_R89_THRESHOLD_MASK << shi)) >> shi)

// Register #10 and #11
#define AGET_R1011_INHIBIT_MASK 0x0003

#define AGET_R1011_PUT_INHIBIT(r1011, shi, inh) ((r1011 & (~(AGET_R1011_INHIBIT_MASK << shi))) | ((inh & AGET_R1011_INHIBIT_MASK) << shi))
#define AGET_R1011_GET_INHIBIT(r1011, shi) ((r1011 & (AGET_R1011_INHIBIT_MASK << shi)) >> shi)

// Conversion helper functions
unsigned short AgetChannelNumber2DaqChannelIndex(unsigned short cha_num, unsigned short rst_len);

// Functions to read/write slow registers of the AGET chip
int Aget_VerifiedWrite(Feminos* fem, unsigned short aget, unsigned short adr, unsigned short* val);
int Aget_Write(Feminos* fem, unsigned short aget, unsigned short adr, unsigned short* val);
int Aget_Read(Feminos* fem, unsigned short aget, unsigned short adr, unsigned short* val);

#endif
