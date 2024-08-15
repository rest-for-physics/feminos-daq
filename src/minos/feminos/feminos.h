/*******************************************************************************

                           _____________________

 File:        feminos.h

 Description: Header file for Feminos board


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  June 2011: created

  June 2014: increased the number of registers from 8 to 10

*******************************************************************************/
#ifndef FEMINOS_H
#define FEMINOS_H

#include "blendasic.h"

#define MAX_NB_OF_ASIC_PER_FEMINOS 4
#define ASIC_MAX_CHAN_INDEX 78

#define NB_OF_FEMINOS_REGISTERS 10

/* Register 0 definition and macros */
#define R0_SCA_CNT_M 0x000003FF
#define R0_MODE_AFTER 0x00000400
#define R0_RST_CHAN_CNT 0x00000800
#define R0_FORCEON_ALL 0x00001000
#define R0_ZERO_SUPPRESS 0x00002000
#define R0_PED_SUBTRACT 0x00004000
#define R0_EMIT_CH_HIT_CNT 0x00008000
#define R0_KEEP_RST_CHAN 0x00010000
#define R0_SKIP_RST_UNTIL_M 0x00060000
#define R0_MODIFY_HIT_REG 0x00080000
#define R0_SCA_ENABLE 0x00100000
#define R0_SCA_AUTO_START 0x00200000
#define R0_SCA_START 0x00400000
#define R0_SCA_STOP 0x00800000
#define R0_CLR_TSTAMP 0x01000000
#define R0_CLR_EVCNT 0x02000000
#define R0_BIOS_B 0x04000000
#define R0_CARD_IX_M 0xF8000000

#define R0_PUT_SCA_CNT(r0, cnt) (((r0) & 0xFFFFFC00) | ((cnt) & 0x000003FF))
#define R0_GET_SCA_CNT(r0) ((r0) & 0x000003FF)
#define R0_GET_MODE(r0) (((r0) & R0_MODE_AFTER) >> 10)
#define R0_GET_RST_CHAN_CNT(r0) (((r0) & R0_RST_CHAN_CNT) >> 11)
#define R0_GET_FORCEON_ALL(r0) (((r0) & R0_FORCEON_ALL) >> 12)
#define R0_GET_ZERO_SUPPRESS(r0) (((r0) & R0_ZERO_SUPPRESS) >> 13)
#define R0_GET_PED_SUBTRACT(r0) (((r0) & R0_PED_SUBTRACT) >> 14)
#define R0_GET_EMIT_CH_HIT_CNT(r0) (((r0) & R0_EMIT_CH_HIT_CNT) >> 15)
#define R0_GET_KEEP_RST_CHAN(r0) (((r0) & R0_KEEP_RST_CHAN) >> 16)
#define R0_PUT_SKIP_RST_UNTIL(r0, skp) (((r0) & 0xFFF9FFFF) | (((skp) & 0x000003) << 17))
#define R0_GET_SKIP_RST_UNTIL(r0) (((r0) >> 17) & 0x000003)
#define R0_GET_MODIFY_HIT_REG(r0) (((r0) & R0_MODIFY_HIT_REG) >> 19)
#define R0_GET_SCA_ENABLE(r0) (((r0) & R0_SCA_ENABLE) >> 20)
#define R0_GET_SCA_AUTO_START(r0) (((r0) & R0_SCA_AUTO_START) >> 21)
// SFT_SCA_START  : bit 22
// SFT_SCA_STOP   : bit 23
// SFT_CLR_TSTAMP : bit 24
// SFT_CLR_EVCNT  : bit 25
// BIOS_B         : bit 26
#define R0_PUT_CARD_IX(r0, ix) (((r0) & 0x07FFFFFF) | (((ix) & 0x00001F) << 27))
#define R0_GET_CARD_IX(r0) (((r0) >> 27) & 0x00001F)

/* Register 1 definition and macros */
#define R1_ASFC_CS_M 0x0000000F
#define R1_ASFC_MISO_M 0x000000F0
#define R1_ASFC_MOSI 0x00000100
#define R1_ASFC_SCLK 0x00000200
#define R1_SC_REQ 0x00000400
#define R1_GEN_CS 0x00000800
#define R1_PUL_LOAD 0x00001000
#define R1_SC_GRANT 0x00008000
#define R1_SYNCH_REQ 0x00010000
#define R1_KEEP_FCO 0x00020000
#define R1_FEC_ENABLE 0x00040000
#define R1_FEC_POW_INV 0x00080000
#define R1_ASIC_MASK_M 0x00F00000
#define R1_TIME_PROBE 0x80000000

#define R1_PUT_ASFC_CS(r1, cs) (((r1) & (~R1_ASFC_CS_M)) | ((cs) & R1_ASFC_CS_M))
#define R1_GET_ASFC_MISO(r1) (((r1) & R1_ASFC_MISO_M) >> 4)
#define R1_PUT_ASFC_MOSI(r1, mo) (((r1) & (~R1_ASFC_MOSI)) | (((mo) << 8) & R1_ASFC_MOSI))
#define R1_PUT_ASFC_SCLK(r1, ck) (((r1) & (~R1_ASFC_SCLK)) | (((ck) << 9) & R1_ASFC_SCLK))
#define R1_PUT_SC_REQ(r1, rq) (((r1) & (~R1_SC_REQ)) | (((rq) << 10) & R1_SC_REQ))
#define R1_GET_SC_GRANT(r1) (((r1) & R1_SC_GRANT) >> 15)
#define R1_PUT_KEEP_FCO(r1, fco) (((r1) & (~R1_KEEP_FCO)) | (((fco) << 17) & R1_KEEP_FCO))
#define R1_GET_KEEP_FCO(r1) (((r1) & R1_KEEP_FCO) >> 17)
#define R1_PUT_FEC_ENABLE(r1, fen) (((r1) & (~R1_FEC_ENABLE)) | (((fen) << 18) & R1_FEC_ENABLE))
#define R1_GET_FEC_ENABLE(r1) (((r1) & R1_FEC_ENABLE) >> 18)
#define R1_PUT_FEC_POW_INV(r1, fen) (((r1) & (~R1_FEC_POW_INV)) | (((fen) << 19) & R1_FEC_POW_INV))
#define R1_GET_FEC_POW_INV(r1) (((r1) & R1_FEC_POW_INV) >> 19)
#define R1_PUT_ASIC_MASK(r1, ma) (((r1) & (~R1_ASIC_MASK_M)) | (((ma) << 20) & R1_ASIC_MASK_M))
#define R1_GET_ASIC_MASK(r1) (((r1) & R1_ASIC_MASK_M) >> 20)

/* Silicon ID chip is controlled by Feminos Register #1 */
#define ADDR_SI_ID_CHIPS 1
#define MSK_SI_ID_CHIPS 0x1F000000
#define SI_ID_CHIPS_MISO 0x10000000
#define SI_ID_CHIPS_BUSY 0x08000000
#define SI_ID_CHIPS_LDACT 0x01000000
#define SI_ID_CHIPS_NOP 0x00000000
#define SI_ID_CHIPS_RESET 0x00000000
#define SI_ID_CHIPS_READ_BIT 0x02000000
#define SI_ID_CHIPS_WRITE_0 0x04000000
#define SI_ID_CHIPS_WRITE_1 0x06000000

/* Register 2 definition and macros */
#define R2_TRIG_DELAY_M 0x0000FFFF
#define R2_TRIG_RATE_M 0x01FF0000
#define R2_EVENT_LIMIT_M 0x0E000000
#define R2_AUTO_TRIG_ENABLE 0x10000000
#define R2_EXT_TRIG_ENABLE 0x20000000
#define R2_PUL_TRIG_ENABLE 0x40000000
#define R2_TRIG_ENABLE_M 0xF0000000

#define R2_PUT_TRIG_DELAY(r2, del) (((r2) & (~R2_TRIG_DELAY_M)) | ((del) & R2_TRIG_DELAY_M))
#define R2_GET_TRIG_DELAY(r2) ((r2) & R2_TRIG_DELAY_M)
#define R2_PUT_TRIG_RATE(r2, rat) (((r2) & (~R2_TRIG_RATE_M)) | (((rat) << 16) & R2_TRIG_RATE_M))
#define R2_GET_TRIG_RATE(r2) (((r2) & R2_TRIG_RATE_M) >> 16)
#define R2_PUT_TRIG_ENABLE(r2, ena) (((r2) & (~R2_TRIG_ENABLE_M)) | (((ena) << 28) & R2_TRIG_ENABLE_M))
#define R2_GET_TRIG_ENABLE(r2) (((r2) & R2_TRIG_ENABLE_M) >> 28)
#define R2_PUT_EVENT_LIMIT(r2, lim) (((r2) & (~R2_EVENT_LIMIT_M)) | (((lim) << 25) & R2_EVENT_LIMIT_M))
#define R2_GET_EVENT_LIMIT(r2) (((r2) & R2_EVENT_LIMIT_M) >> 25)

/* Register 3 definition and macros */
#define R3_PUL_DELAY_M 0x00000FFF
#define R3_PUL_ENABLE 0x00001000
#define R3_MULT_TRIG_ENA 0x00002000
#define R3_SND_MULT_ENA 0x00004000
#define R3_ERASE_HIT_ENA 0x00008000
#define R3_POLARITY_0 0x00010000
#define R3_POLARITY_M 0x000F0000
#define R3_ERASE_HIT_THR_0_M 0x00700000
#define R3_ERASE_HIT_THR_M 0xFFF00000

#define R3_PUT_PUL_DELAY(r3, del) (((r3) & (~R3_PUL_DELAY_M)) | ((del) & R3_PUL_DELAY_M))
#define R3_GET_PUL_DELAY(r3) ((r3) & R3_PUL_DELAY_M)
#define R3_PUT_PUL_ENABLE(r3, ena) (((r3) & (~R3_PUL_ENABLE)) | ((ena << 12) & R3_PUL_ENABLE))
#define R3_GET_PUL_ENABLE(r3) (((r3) & R3_PUL_ENABLE) >> 12)
#define R3_PUT_MULT_TRIG_ENA(r3, ena) (((r3) & (~R3_MULT_TRIG_ENA)) | ((ena << 13) & R3_MULT_TRIG_ENA))
#define R3_GET_MULT_TRIG_ENA(r3) (((r3) & R3_MULT_TRIG_ENA) >> 13)
#define R3_PUT_SND_MULT_ENA(r3, ena) (((r3) & (~R3_SND_MULT_ENA)) | ((ena << 14) & R3_SND_MULT_ENA))
#define R3_GET_SND_MULT_ENA(r3) (((r3) & R3_SND_MULT_ENA) >> 14)
#define R3_PUT_ERASE_HIT_ENA(r3, ena) (((r3) & (~R3_ERASE_HIT_ENA)) | ((ena << 15) & R3_ERASE_HIT_ENA))
#define R3_GET_ERASE_HIT_ENA(r3) (((r3) & R3_ERASE_HIT_ENA) >> 15)
#define R3_PUT_POLARITY(r3, asi, pol) (((r3) & ~(R3_POLARITY_0 << asi)) | ((pol << (16 + asi)) & (R3_POLARITY_0 << asi)))
#define R3_GET_POLARITY(r3, asi) (((r3) & (R3_POLARITY_0 << asi)) >> (16 + asi))
#define R3_PUT_ERASE_HIT_THR(r3, asi, thr) (((r3) & ~(R3_ERASE_HIT_THR_0_M << (3 * asi))) | ((thr << (20 + (3 * asi))) & (R3_ERASE_HIT_THR_0_M << (3 * asi))))
#define R3_PUT_ERASE_HIT_THR_M(r3, asi) (((r3) & (~(R3_ERASE_HIT_THR_0_M << (3 * asi)))) | (R3_ERASE_HIT_THR_0_M << (3 * asi)))
#define R3_GET_ERASE_HIT_THR(r3, asi) (((r3) & (R3_ERASE_HIT_THR_0_M << (3 * asi))) >> (20 + (3 * asi)))

/* Register 4 definition and macros */
#define R4_PUT_MULT_THR(r4, ch, va) (((r4) & (~(0xFF << (8 * ch)))) | ((va & 0xFF) << (8 * ch)))
#define R4_PUT_MULT_THR_M(r4, ch) (((r4) & (~(0xFF << (8 * ch)))) | (0xFF << (8 * ch)))
#define R4_GET_MULT_THR(r4, ch) (((r4) >> (8 * ch)) & 0xFF)

/* Register 5 definition and macros */
#define R5_ZS_POST_PRE_M 0x000001FF
#define R5_TEST_ENABLE 0x00000200
#define R5_TEST_MODE 0x00000400
#define R5_TEST_ZBT 0x00000800
#define R5_GEN_USE_LIMIT 0x00001000
#define R5_EMIT_LST_CELL_RD 0x00002000
#define R5_EOF_ON_EOE 0x00004000
#define R5_EMIT_EMPTY_CH 0x00008000
#define R5_WCKDIV_M 0x003F0000
#define R5_STATE_M 0xFF000000
#define R5_ALIGNED 0x01000000
#define R5_SCA_WRITE 0x02000000
#define R5_SCA_READ 0x04000000
#define R5_DEV_BUSY 0x08000000
#define R5_DEV_READY 0x10000000
#define R5_ZBT_FULL 0x20000000
#define R5_EVF_FULL 0x40000000
#define R5_RBF_OFIFO_EMPTY 0x80000000

#define R5_PUT_ZS_POST_PRE(r5, po, pr) (((r5) & (~R5_ZS_POST_PRE_M)) | (((po) & 0x1F) << 4) | ((pr) & 0x0F))
#define R5_GET_ZS_POST(r5) (((r5) >> 4) & 0x1F)
#define R5_GET_ZS_PRE(r5) (((r5) >> 0) & 0x0F)
#define R5_GET_TEST_ENABLE(r5) (((r5) & R5_TEST_ENABLE) >> 9)
#define R5_GET_TEST_MODE(r5) (((r5) & R5_TEST_MODE) >> 10)
#define R5_GET_TEST_ZBT(r5) (((r5) & R5_TEST_ZBT) >> 11)
#define R5_GET_GEN_USE_LIMIT(r5) (((r5) & R5_GEN_USE_LIMIT) >> 12)
#define R5_GET_EMIT_LST_CELL_RD(r5) (((r5) & R5_EMIT_LST_CELL_RD) >> 13)
#define R5_GET_EOF_ON_EOE(r5) (((r5) & R5_EOF_ON_EOE) >> 14)
#define R5_GET_EMIT_EMPTY_CH(r5) (((r5) & R5_EMIT_EMPTY_CH) >> 15)
#define R5_PUT_WCKDIV(r5, dv) (((r5) & (~R5_WCKDIV_M)) | (((dv) << 16) & R5_WCKDIV_M))
#define R5_GET_WCKDIV(r5) (((r5) & R5_WCKDIV_M) >> 16)
#define R5_GET_STATE(r5) (((r5) & R5_STATE_M) >> 24)

/* Register 6 definition and macros */
#define R6_RX_COUNT_M 0x000000FF
#define R6_RX_PERRROR_M 0x0000FF00
#define R6_TX_COUNT_M 0x00FF0000
#define R6_DCBAL_ENC 0x01000000
#define R6_DCBAL_DEC 0x02000000
#define R6_TCM_IGNORE 0x04000000
#define R6_TSTAMP_CLR_ISSET 0x08000000
#define R6_TSTAMP_ISSET 0x08000000
#define R6_TS_INIT_SCLK 0x10000000
#define R6_TS_INIT_SDAT 0x20000000
#define R6_TCM_BERT 0x40000000
#define R6_CLR_COUNT 0x80000000

#define R6_GET_RX_COUNT(r6) (((r6) & R6_RX_COUNT_M) >> 0)
#define R6_GET_RX_PERROR(r6) (((r6) & R6_RX_PERRROR_M) >> 8)
#define R6_GET_TX_COUNT(r6) (((r6) & R6_TX_COUNT_M) >> 16)
#define R6_PUT_DCBAL_ENC(r6, enc) (((r6) & (~R6_DCBAL_ENC)) | (((enc) << 24) & R6_DCBAL_ENC))
#define R6_GET_DCBAL_ENC(r6) (((r6) & R6_DCBAL_ENC) >> 24)
#define R6_PUT_DCBAL_DEC(r6, dec) (((r6) & (~R6_DCBAL_DEC)) | (((dec) << 25) & R6_DCBAL_DEC))
#define R6_GET_DCBAL_DEC(r6) (((r6) & R6_DCBAL_DEC) >> 25)
#define R6_PUT_TCM_IGNORE(r6, ign) (((r6) & (~R6_TCM_IGNORE)) | (((ign) << 26) & R6_TCM_IGNORE))
#define R6_GET_TCM_IGNORE(r6) (((r6) & R6_TCM_IGNORE) >> 26)
#define R6_GET_TSTAMP_ISSET(r6) (((r6) & R6_TSTAMP_ISSET) >> 27)
#define R6_PUT_TCM_BERT(r6, brt) (((r6) & (~R6_TCM_BERT)) | (((brt) << 30) & R6_TCM_BERT))
#define R6_GET_TCM_BERT(r6) (((r6) & R6_TCM_BERT) >> 30)
#define R6_PUT_CLR_COUNT(r6, clr) (((r6) & (~R6_CLR_COUNT)) | (((clr) << 31) & R6_CLR_COUNT))

/* Register 8 definition and macros */
#define R8_PUT_MULT_LIMIT(r8, ch, va) (((r8) & (~(0xFF << (8 * ch)))) | ((va & 0xFF) << (8 * ch)))
#define R8_PUT_MULT_LIMIT_M(r8, ch) (((r8) & (~(0xFF << (8 * ch)))) | (0xFF << (8 * ch)))
#define R8_GET_MULT_LIMIT(r8, ch) (((r8) >> (8 * ch)) & 0xFF)

/* Register 9 definition and macros */

typedef struct _Feminos {
    char name[4];
    int card_id;
    volatile unsigned int* reg[NB_OF_FEMINOS_REGISTERS];
    BlendAsic asics[MAX_NB_OF_ASIC_PER_FEMINOS];
} Feminos;

typedef enum _FmRgAction {
    FM_REG_READ,
    FM_REG_WRITE,
    FM_REG_MODIFY
} FmRgAction;

int Feminos_Open(Feminos* fem, int id);
int Feminos_Close(Feminos* fem, int id);
int Feminos_ActOnRegister(Feminos* fem, FmRgAction op, int reg, unsigned int bitsel, unsigned int* dat);
void fstate2s(char* s, unsigned int val);

#endif
