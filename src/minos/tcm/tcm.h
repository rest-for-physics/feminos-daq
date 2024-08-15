/*******************************************************************************

                           _____________________

 File:        tcm.h

 Description: Header file for Trigger Clock Module


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  April 2012: created

  January 2014: added pll and pll_out_delay to Tcm structure

  June 2014: added register 28, i.e. 29th register.

   June 2014: added register 29, i.e. 30th register.

*******************************************************************************/
#ifndef TCM_H
#define TCM_H

#define NB_OF_FEMINOS_PORTS 24

#define NB_OF_TCM_REGISTERS 30

/* Definitions and Macros for Register 20 */
#define R20_ISOBUS_M 0x0000007F
#define R20_EV_TYPE_M 0x00000003
#define R20_EV_TYPE_00 0x00000000
#define R20_EV_TYPE_01 0x00000001
#define R20_EV_TYPE_10 0x00000002
#define R20_EV_TYPE_11 0x00000003
#define R20_CLR_TSTAMP 0x00000004
#define R20_CLR_EVCNT 0x00000008
#define R20_SCA_STOP 0x00000010
#define R20_SCA_START 0x00000020
#define R20_WCK_SYNCH 0x00000040
#define R20_RESTART 0x00000100
#define R20_UW_CLK 0x00000400
#define R20_UW_DATA 0x00000800
#define R20_UW_LE 0x00001000
#define R20_LMK_GOE 0x00002000
#define R20_LMK_SYNC 0x00004000
#define R20_LMK_LD 0x00008000
#define R20_STANDBY 0x00010000
#define R20_WAITING_TRIG 0x00020000
#define R20_WAITING_LAT 0x00040000
#define R20_FEM_BUSY 0x00080000
#define R20_START_ACK_MISS 0x00100000
#define R20_TRIG_ACK_MISS 0x00200000
#define R20_NO_BUSY_MISS 0x00400000

/* Definitions and Macros for Register 22 */
#define R22_TRIG_RATE_M 0x000001FF
#define R22_EVENT_LIMIT_M 0x00000E00
#define R22_TRIG_ENABLE_M 0x0000F000
#define R22_AUTO_TRIG_ENABLE 0x00001000
#define R22_NIM_TRIG_ENABLE 0x00002000
#define R22_LVDS_TRIG_ENABLE 0x00004000
#define R22_TTL_TRIG_ENABLE 0x00008000
#define R22_SEL_EDGE 0x00010000
#define R22_INV_TCM_SIG 0x00020000
#define R22_DO_END_OF_BUSY 0x00040000
#define R22_INV_TCM_CLK 0x00080000
#define R22_MAX_READOUT_TIME_M 0x00F00000
#define R22_DCBAL_ENC 0x01000000
#define R22_DCBAL_DEC 0x02000000
#define R22_MULT_TRIG_ENA 0x04000000
#define R22_MULT_TRIG_DST 0x08000000
#define R22_TCM_BERT 0x10000000
#define R22_BIOS_B 0x80000000

#define R22_PUT_TRIG_RATE(r22, rat) (((r22) & (~R22_TRIG_RATE_M)) | (((rat) << 0) & R22_TRIG_RATE_M))
#define R22_GET_TRIG_RATE(r22) (((r22) & R22_TRIG_RATE_M) >> 0)
#define R22_PUT_EVENT_LIMIT(r22, lim) (((r22) & (~R22_EVENT_LIMIT_M)) | (((lim) << 9) & R22_EVENT_LIMIT_M))
#define R22_GET_EVENT_LIMIT(r22) (((r22) & R22_EVENT_LIMIT_M) >> 9)
#define R22_PUT_TRIG_ENABLE(r22, ena) (((r22) & (~R22_TRIG_ENABLE_M)) | (((ena) << 12) & R22_TRIG_ENABLE_M))
#define R22_GET_TRIG_ENABLE(r22) (((r22) & R22_TRIG_ENABLE_M) >> 12)
#define R22_PUT_SEL_EDGE(r22, sel) (((r22) & (~R22_SEL_EDGE)) | (((sel) << 16) & R22_SEL_EDGE))
#define R22_GET_SEL_EDGE(r22) (((r22) & R22_SEL_EDGE) >> 16)
#define R22_PUT_INV_TCM_SIG(r22, inv) (((r22) & (~R22_INV_TCM_SIG)) | (((inv) << 17) & R22_INV_TCM_SIG))
#define R22_GET_INV_TCM_SIG(r22) (((r22) & R22_INV_TCM_SIG) >> 17)
#define R22_PUT_DO_END_OF_BUSY(r22, eob) (((r22) & (~R22_DO_END_OF_BUSY)) | (((eob) << 18) & R22_DO_END_OF_BUSY))
#define R22_GET_DO_END_OF_BUSY(r22) (((r22) & R22_DO_END_OF_BUSY) >> 18)
#define R22_PUT_INV_TCM_CLK(r22, inv) (((r22) & (~R22_INV_TCM_CLK)) | (((inv) << 19) & R22_INV_TCM_CLK))
#define R22_GET_INV_TCM_CLK(r22) (((r22) & R22_INV_TCM_CLK) >> 19)
#define R22_PUT_MAX_READOUT_TIME(r22, mr) (((r22) & (~R22_MAX_READOUT_TIME_M)) | (((mr) << 20) & R22_MAX_READOUT_TIME_M))
#define R22_GET_MAX_READOUT_TIME(r22) (((r22) & R22_MAX_READOUT_TIME_M) >> 20)
#define R22_PUT_DCBAL_ENC(r22, enc) (((r22) & (~R22_DCBAL_ENC)) | (((enc) << 24) & R22_DCBAL_ENC))
#define R22_GET_DCBAL_ENC(r22) (((r22) & R22_DCBAL_ENC) >> 24)
#define R22_PUT_DCBAL_DEC(r22, dec) (((r22) & (~R22_DCBAL_DEC)) | (((dec) << 25) & R22_DCBAL_DEC))
#define R22_GET_DCBAL_DEC(r22) (((r22) & R22_DCBAL_DEC) >> 25)
#define R22_PUT_MULT_TRIG_ENA(r22, ena) (((r22) & (~R22_MULT_TRIG_ENA)) | (((ena) << 26) & R22_MULT_TRIG_ENA))
#define R22_GET_MULT_TRIG_ENA(r22) (((r22) & R22_MULT_TRIG_ENA) >> 26)
#define R22_PUT_MULT_TRIG_DST(r22, dst) (((r22) & (~R22_MULT_TRIG_DST)) | (((dst) << 27) & R22_MULT_TRIG_DST))
#define R22_GET_MULT_TRIG_DST(r22) (((r22) & R22_MULT_TRIG_DST) >> 27)
#define R22_PUT_TCM_BERT(r22, ber) (((r22) & (~R22_TCM_BERT)) | (((ber) << 28) & R22_TCM_BERT))
#define R22_GET_TCM_BERT(r22) (((r22) & R22_TCM_BERT) >> 28)

/* Definitions and Macros for Register 24 */
#define R24_PUT_TRIG_DELAY_L(r24, dly) (((r24) & (~0x0000FFFF)) | (((dly) << 0) & 0x0000FFFF))
#define R24_PUT_TRIG_DELAY_H(r24, dly) (((r24) & (~0xFFFF0000)) | (((dly) << 16) & 0xFFFF0000))
#define R24_GET_TRIG_DELAY_L(r24) (((r24) & 0x0000FFFF) >> 0)
#define R24_GET_TRIG_DELAY_H(r24) (((r24) & 0xFFFF0000) >> 16)

/* Definitions and Macros for Register 25 */
#define R25_PUT_TRIG_DELAY_L(r25, dly) (((r25) & (~0x0000FFFF)) | (((dly) << 0) & 0x0000FFFF))
#define R25_PUT_TRIG_DELAY_H(r25, dly) (((r25) & (~0xFFFF0000)) | (((dly) << 16) & 0xFFFF0000))
#define R25_GET_TRIG_DELAY_L(r25) (((r25) & 0x0000FFFF) >> 0)
#define R25_GET_TRIG_DELAY_H(r25) (((r25) & 0xFFFF0000) >> 16)

/* Definitions and Macros for Register 28 */
#define R28_FEM_MASK_M ((1 << NB_OF_FEMINOS_PORTS) - 1)

/* Definitions and Macros for Register 29 */
#define R29_MULT_MORE_THAN_M 0x0000007F
#define R29_MULT_LESS_THAN_M 0x00003F80
#define R29_PUT_MULT_MORE_THAN(r29, thr) (((r29) & (~R29_MULT_MORE_THAN_M)) | (((thr) << 0) & R29_MULT_MORE_THAN_M))
#define R29_GET_MULT_MORE_THAN(r29) (((r29) & R29_MULT_MORE_THAN_M) >> 0)
#define R29_PUT_MULT_LESS_THAN(r29, thr) (((r29) & (~R29_MULT_LESS_THAN_M)) | (((thr) << 7) & R29_MULT_LESS_THAN_M))
#define R29_GET_MULT_LESS_THAN(r29) (((r29) & R29_MULT_LESS_THAN_M) >> 7)

typedef struct _Tcm {
    int card_id;
    char name[4];
    volatile unsigned int* reg[NB_OF_TCM_REGISTERS];
    unsigned char pll;      // PLL present/not present (TCM only)
    unsigned char pll_odel; // PLL output clock fine delay (TCM with PLL only)
} Tcm;

typedef enum _TcmRgAction {
    TCM_REG_READ,
    TCM_REG_WRITE,
    TCM_REG_MODIFY
} TcmRgAction;

int Tcm_Open(Tcm* tcm);
int Tcm_Close(Tcm* tcm);
int Tcm_ActOnRegister(Tcm* tcm, TcmRgAction op, int reg, unsigned int bitsel, unsigned int* dat);

#endif
