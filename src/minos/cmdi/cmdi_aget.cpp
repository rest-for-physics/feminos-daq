/*******************************************************************************

                           _____________________

 File:        cmdi_aget.c

 Description: Command interpreter of the Feminos card.
  This file implements the commands specific to the control of the AGET chip

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr

 History:
  November 2011: created

  February 2014: corrected error: uninitialized argument to specify which
  register to read for command "aget x mode" and "aget x test".

  March 2014: added command "aget x icsa"

  March 2014: added command "aget x cur_ra" and "aget x cur_buf"

*******************************************************************************/

#include "aget.h"
#include "aget_strings.h"
#include "cmdi.h"
#include "error_codes.h"
#include "hitscurve.h"

#include <cstdio>
#include <string.h>

/*******************************************************************************
 Cmdi_AgetChannelCommands
*******************************************************************************/
int Cmdi_AgetChannelCommands(CmdiContext* c) {
    int arg_cnt;
    int is_mult;
    int act_cnt;
    int una_cnt;
    int err;
    unsigned short asic_cur, asic_beg, asic_end;
    unsigned short cha_cur, cha_beg, cha_end;
    unsigned short hix;
    char str[8][16];
    int param[10];
    unsigned short val_wrs;
    char* rep;
    unsigned short reg_adr;
    unsigned short reg_sho;
    unsigned short reg_bit;
    unsigned short reg_msk;
    unsigned short val_rd[8];
    unsigned short reg_data;
    unsigned short val_wr_A[8];
    unsigned short val_wr_B[8];
    unsigned short reg_mod;
    int i, j;
    int action;
    unsigned short reg_adr_A, reg_adr_B;
    unsigned short reg_cnt;
    unsigned short reg_size;
    unsigned short reg_mult;
    unsigned short prob_hit;
    int thr_inc;

    // Init
    err = 0;
    asic_beg = 0;
    asic_end = 0;
    cha_beg = 0;
    cha_end = 0;
    is_mult = 0;
    thr_inc = 0;

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME
    c->do_reply = 1;

    // Scan arguments
    if ((arg_cnt = sscanf(c->cmd, "%s %s %s %s %s", &str[0][0], &str[1][0], &str[2][0], &str[3][0], &str[4][0])) >= 4) {
        // Wildcard on ASIC?
        if (str[1][0] == '*') {
            asic_beg = 0;
            asic_end = MAX_NB_OF_AGET_PER_FEMINOS - 1;
            is_mult = 1;
        } else {
            // Scan ASIC index range
            if (sscanf(&str[1][0], "%i:%i", &param[0], &param[1]) == 2) {
                asic_beg = (unsigned short) param[0];
                asic_end = (unsigned short) param[1];
                is_mult = 1;
            }
            // Scan ASIC index
            else if (sscanf(&str[1][0], "%i", &param[0]) == 1) {
                asic_beg = (unsigned short) param[0];
                asic_end = (unsigned short) param[0];
            } else {
                err = -1;
            }
        }

        // Wildcard on Channel?
        if (str[3][0] == '*') {
            cha_beg = 1; // Real AGET channels start at 1
            cha_end = MAX_AGET_REAL_CHAN_CNT;
            is_mult = 1;
        } else {
            // Scan Channel index range
            if (sscanf(&str[3][0], "%i:%i", &param[0], &param[1]) == 2) {
                cha_beg = (unsigned short) param[0];
                cha_end = (unsigned short) param[1];
                is_mult = 1;
            }
            // Scan Channel index
            else if (sscanf(&str[3][0], "%i", &param[0]) == 1) {
                cha_beg = (unsigned short) param[0];
                cha_end = (unsigned short) param[0];
            } else {
                err = -1;
            }
        }
    } else {
        err = -1;
    }

    // Scan arguments for write operation
    thr_inc = 0;
    if (arg_cnt == 5) {
        if (strncmp(&str[4][0], "++", 2) == 0) {
            thr_inc = 1;
        } else if (strncmp(&str[4][0], "--", 2) == 0) {
            thr_inc = -1;
        } else if (sscanf(&str[4][0], "%i", &param[3]) != 1) {
            err = -1;
        }
    }

    // Swap begin and end arguments if not in order
    if (asic_beg > asic_end) {
        asic_cur = asic_end;
        asic_end = asic_beg;
        asic_beg = asic_cur;
    }
    if (cha_beg > cha_end) {
        cha_cur = cha_end;
        cha_end = cha_beg;
        cha_beg = cha_cur;
    }

    // Check that supplied arguments are within acceptable range
    if ((err == -1) ||
        (asic_end >= MAX_NB_OF_AGET_PER_FEMINOS) ||
        (cha_beg < 1) ||
        (cha_end > MAX_AGET_REAL_CHAN_CNT) ||
        ((arg_cnt == 4) && (is_mult == 1))) {
        err = -1;
        sprintf(rep, "%d Fem(%02d)%s <Id> <Action> <Channel> <Value>\n", err, c->fem->card_id, &str[0][0]);
        return (err);
    }

    // Analyze the required action
    if (strncmp(&str[2][0], "gain", 4) == 0) {
        action = 1;
    } else if (strncmp(&str[2][0], "inhibit", 7) == 0) {
        action = 2;
    } else if (strncmp(&str[2][0], "threshold", 9) == 0) {
        action = 3;
    } else if (strncmp(&str[2][0], "hitprob", 7) == 0) {
        action = 4;
    } else {
        err = -1;
        sprintf(rep, "%d Fem(%02d) %s %s %s: unsupported action\n", err, c->fem->card_id, &str[0][0], &str[1][0], &str[2][0]);
        return (err);
    }

    // Read single entry
    if (arg_cnt == 4) {
        // Read channel gain
        if (action == 1) {
            if (cha_beg <= 32) {
                reg_adr = 6;
                reg_sho = (64 - 2 * cha_beg) >> 4;
                reg_bit = (64 - 2 * cha_beg) % 16;
                reg_msk = AGET_R67_GAIN_MASK;
            } else {
                reg_adr = 7;
                reg_sho = (2 * (cha_beg - 33)) >> 4;
                reg_bit = (2 * (cha_beg - 33)) % 16;
                reg_msk = AGET_R67_GAIN_MASK;
            }

            // Read AGET register 6 or 7 (64-bit)
            if ((err = Aget_Read(c->fem, asic_beg, reg_adr, &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) Aget(%d) Register(%d): Aget_Read failed\n",
                        err,
                        c->fem->card_id,
                        asic_beg,
                        reg_adr);
            } else {
                reg_data = AGET_R67_GET_GAIN(c->fem->asics[asic_beg].bar[reg_adr].bars[reg_sho], reg_bit);
                sprintf(rep, "0 Fem(%02d) Aget(%d) Channel(%d) Gain = 0x%x (%s)\n",
                        c->fem->card_id,
                        asic_beg,
                        cha_beg,
                        reg_data,
                        &(Aget_ChannelGain2str[reg_data][0]));
                return (0);
            }
        }
        // Read channel inhibit
        else if (action == 2) {
            if (cha_beg <= 32) {
                reg_adr = 10;
                reg_sho = (64 - 2 * cha_beg) >> 4;
                reg_bit = (64 - 2 * cha_beg) % 16;
                reg_msk = AGET_R1011_INHIBIT_MASK;
            } else {
                reg_adr = 11;
                reg_sho = (2 * (cha_beg - 33)) >> 4;
                reg_bit = (2 * (cha_beg - 33)) % 16;
                reg_msk = AGET_R1011_INHIBIT_MASK;
            }

            // Read AGET register 10 or 11 (64-bit)
            if ((err = Aget_Read(c->fem, asic_beg, reg_adr, &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) Aget(%d) Register(%d): Aget_Read failed\n",
                        err,
                        c->fem->card_id,
                        asic_beg,
                        reg_adr);
            } else {
                reg_data = AGET_R1011_GET_INHIBIT(c->fem->asics[asic_beg].bar[reg_adr].bars[reg_sho], reg_bit);
                sprintf(rep, "0 Fem(%02d) Aget(%d) Channel(%d) Inhibit = 0x%x (%s)\n",
                        c->fem->card_id,
                        asic_beg,
                        cha_beg,
                        reg_data,
                        &(Aget_ChannelInhibit2str[reg_data][0]));
                return (0);
            }
        }
        // Read channel threshold
        else if ((action == 3) || (action == 4)) {
            if (cha_beg <= 32) {
                reg_adr = 8;
                reg_sho = (128 - 4 * cha_beg) >> 4;
                reg_bit = (128 - 4 * cha_beg) % 16;
                reg_msk = AGET_R89_THRESHOLD_MASK;
            } else {
                reg_adr = 9;
                reg_sho = (4 * (cha_beg - 33)) >> 4;
                reg_bit = (4 * (cha_beg - 33)) % 16;
                reg_msk = AGET_R1011_INHIBIT_MASK;
            }

            // Read AGET register 8 or 9 (128-bit)
            if ((err = Aget_Read(c->fem, asic_beg, reg_adr, &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) Aget(%d) Register(%d): Aget_Read failed\n",
                        err,
                        c->fem->card_id,
                        asic_beg,
                        reg_adr);
            } else {
                reg_data = AGET_R89_GET_THRESHOLD(c->fem->asics[asic_beg].bar[reg_adr].bars[reg_sho], reg_bit);
                sprintf(rep, "0 Fem(%02d) Aget(%d) Channel(%d) Threshold = 0x%x (%d)\n",
                        c->fem->card_id,
                        asic_beg,
                        cha_beg,
                        reg_data,
                        reg_data);
                return (0);
            }
        } else {
            err = -1;
            sprintf(rep, "%d Fem(%02d) aget %d %s : action not supported\n", err, c->fem->card_id, asic_beg, &str[2][0]);
            return (0);
        }
    }

    // Write multiple gain or threshold entries
    if (action == 4) {
        val_wrs = 0; // will be assigned later
        prob_hit = (unsigned short) param[3];
    } else {
        val_wrs = (unsigned short) param[3];
        prob_hit = 0; // unused in this case
    }
    act_cnt = 0;
    una_cnt = 0;

    // Prepare arguments
    if (action == 1) {
        reg_adr_A = 6;
        reg_adr_B = 7;
        reg_cnt = 4;
        reg_size = 64;
        reg_mult = 2;
    } else if (action == 2) {
        reg_adr_A = 10;
        reg_adr_B = 11;
        reg_cnt = 4;
        reg_size = 64;
        reg_mult = 2;
    } else if (action == 3) {
        reg_adr_A = 8;
        reg_adr_B = 9;
        reg_cnt = 8;
        reg_size = 128;
        reg_mult = 4;
    } else if (action == 4) {
        reg_adr_A = 8;
        reg_adr_B = 9;
        reg_cnt = 8;
        reg_size = 128;
        reg_mult = 4;
    } else {
        // never come here
        reg_adr_A = 0;
        reg_adr_B = 0;
        reg_cnt = 0;
        reg_size = 0;
        reg_mult = 0;
    }

    for (asic_cur = asic_beg; asic_cur <= asic_end; asic_cur++) {
        // Get a copy of mirror registers
        for (i = 0; i < reg_cnt; i++) {
            val_wr_A[reg_cnt - i - 1] = c->fem->asics[asic_cur].bar[reg_adr_A].bars[i];
            val_wr_B[reg_cnt - i - 1] = c->fem->asics[asic_cur].bar[reg_adr_B].bars[i];
        }

        // Prepare data to be written
        reg_mod = 0;
        for (cha_cur = cha_beg; cha_cur <= cha_end; cha_cur++) {
            // Determine channel threshold given the desired hit probability
            if (action == 4) {
                // Loop over all possible thresholds
                for (j = 0; j < 16; j++) {
                    val_wrs = (unsigned short) j;
                    hix = AgetChannelNumber2DaqChannelIndex(cha_cur, 1);
                    if (fshisto.asicshisto[asic_cur].shisto[hix].s_bins[j] < prob_hit) {
                        break;
                    }
                }
                // check if we were able to set the threshold
                if (j == 16) {
                    una_cnt++;
                }
            }

            // Determine to which 16-bit word the action applies
            if (cha_cur <= 32) {
                reg_mod |= 0x1;
                reg_sho = (reg_size - reg_mult * cha_cur) >> 4;
                reg_bit = (reg_size - reg_mult * cha_cur) % 16;
            } else {
                reg_mod |= 0x2;
                reg_sho = (reg_mult * (cha_cur - 33)) >> 4;
                reg_bit = (reg_mult * (cha_cur - 33)) % 16;
            }

            // Increment or decrement threshold
            if ((action == 3) && (thr_inc != 0)) {
                // Get the current value of the threshold
                if (cha_cur <= 32) {
                    reg_data = AGET_R89_GET_THRESHOLD(val_wr_A[reg_cnt - 1 - reg_sho], reg_bit);
                } else {
                    reg_data = AGET_R89_GET_THRESHOLD(val_wr_B[reg_cnt - 1 - reg_sho], reg_bit);
                }
                // Compute new threshold value
                if (thr_inc == 1) {
                    if (reg_data == 0xF) {
                        una_cnt++;
                    } else {
                        reg_data++;
                    }
                } else if (thr_inc == -1) {
                    if (reg_data == 0x0) {
                        una_cnt++;
                    } else {
                        reg_data--;
                    }
                }
                val_wrs = reg_data;
            }

            if (cha_cur <= 32) {
                if (action == 1) {
                    reg_msk = AGET_R67_GAIN_MASK;
                    val_wr_A[reg_cnt - 1 - reg_sho] = AGET_R67_PUT_GAIN(val_wr_A[reg_cnt - 1 - reg_sho], reg_bit, val_wrs);
                } else if (action == 2) {
                    reg_msk = AGET_R1011_INHIBIT_MASK;
                    val_wr_A[reg_cnt - 1 - reg_sho] = AGET_R1011_PUT_INHIBIT(val_wr_A[reg_cnt - 1 - reg_sho], reg_bit, val_wrs);
                } else if (action == 3) {
                    reg_msk = AGET_R89_THRESHOLD_MASK;
                    val_wr_A[reg_cnt - 1 - reg_sho] = AGET_R89_PUT_THRESHOLD(val_wr_A[reg_cnt - 1 - reg_sho], reg_bit, val_wrs);
                } else if (action == 4) {
                    reg_msk = AGET_R89_THRESHOLD_MASK;
                    val_wr_A[reg_cnt - 1 - reg_sho] = AGET_R89_PUT_THRESHOLD(val_wr_A[reg_cnt - 1 - reg_sho], reg_bit, val_wrs);
                }
            } else {
                if (action == 1) {
                    reg_msk = AGET_R67_GAIN_MASK;
                    val_wr_B[reg_cnt - 1 - reg_sho] = AGET_R67_PUT_GAIN(val_wr_B[reg_cnt - 1 - reg_sho], reg_bit, val_wrs);
                } else if (action == 2) {
                    reg_msk = AGET_R1011_INHIBIT_MASK;
                    val_wr_B[reg_cnt - 1 - reg_sho] = AGET_R1011_PUT_INHIBIT(val_wr_B[reg_cnt - 1 - reg_sho], reg_bit, val_wrs);
                } else if (action == 3) {
                    reg_msk = AGET_R89_THRESHOLD_MASK;
                    val_wr_B[reg_cnt - 1 - reg_sho] = AGET_R89_PUT_THRESHOLD(val_wr_B[reg_cnt - 1 - reg_sho], reg_bit, val_wrs);
                } else if (action == 4) {
                    reg_msk = AGET_R89_THRESHOLD_MASK;
                    val_wr_B[reg_cnt - 1 - reg_sho] = AGET_R89_PUT_THRESHOLD(val_wr_B[reg_cnt - 1 - reg_sho], reg_bit, val_wrs);
                }
            }
            act_cnt++;
        }

        // Write and Verify AGET registers
        if (reg_mod & 0x1) {
            if ((err = Aget_VerifiedWrite(c->fem, asic_cur, reg_adr_A, &val_wr_A[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) Aget(%d) Register(%d): Aget_VerifiedWrite failed\n",
                        err,
                        c->fem->card_id,
                        asic_cur,
                        reg_adr_A);
                return (err);
            } else {
                // nothing
            }
        }
        if (reg_mod & 0x2) {
            if ((err = Aget_VerifiedWrite(c->fem, asic_cur, reg_adr_B, &val_wr_B[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) Aget(%d) Register(%d): Aget_VerifiedWrite failed\n",
                        err,
                        c->fem->card_id,
                        asic_cur,
                        reg_adr_B);
                return (err);
            } else {
                // nothing
            }
        }
    }

    // Make result string
    if (action == 4) {
        if (is_mult == 1) {
            sprintf(rep, "0 Fem(%02d) Aget[%d:%d] Channel[%d:%d] HitProb<%d (%d actions %d underrange)\n", c->fem->card_id, asic_beg, asic_end, cha_beg, cha_end, prob_hit, act_cnt, una_cnt);
        } else {
            sprintf(rep, "0 Fem(%02d) Aget[%d] Channel[%d] Threshold <- 0x%x HitProb<%d (%d underrange)\n", c->fem->card_id, asic_beg, cha_beg, val_wrs, prob_hit, una_cnt);
        }
    } else if (action == 3) {
        if (is_mult == 1) {
            sprintf(rep, "0 Fem(%02d) Aget[%d:%d] Channel[%d:%d] %s <- 0x%x (%d actions %d underrange)\n", c->fem->card_id, asic_beg, asic_end, cha_beg, cha_end, &str[2][0], val_wrs, act_cnt, una_cnt);
        } else {
            sprintf(rep, "0 Fem(%02d) Aget[%d] Channel[%d] %s <- 0x%x (%d underrange)\n", c->fem->card_id, asic_beg, cha_beg, &str[2][0], val_wrs, una_cnt);
        }
    } else {
        if (is_mult == 1) {
            sprintf(rep, "0 Fem(%02d) Aget[%d:%d] Channel[%d:%d] %s <- 0x%x (%d actions)\n", c->fem->card_id, asic_beg, asic_end, cha_beg, cha_end, &str[2][0], val_wrs, act_cnt);
        } else {
            sprintf(rep, "0 Fem(%02d) Aget[%d] Channel[%d] %s <- 0x%x\n", c->fem->card_id, asic_beg, cha_beg, &str[2][0], val_wrs);
        }
    }

    return (0);
}

/*******************************************************************************
 Cmdi_AgetCommands
*******************************************************************************/
int Cmdi_AgetCommands(CmdiContext* c) {
    int i;
    unsigned int param[10];
    int err = 0;
    char act_str[32];
    char agt_str[16];
    char arg_str[64];
    unsigned int aget_beg;
    unsigned int aget_end;
    unsigned int aget_tmp;
    unsigned int is_mult;
    unsigned int is_valid;

    char* rep;
    unsigned short val_rd[8];
    unsigned short val_wr[8];

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME

    // Clear array of values read (even if not always needed)
    for (i = 0; i < 8; i++) {
        val_rd[i] = 0x0000;
    }

    // Get operation and target
    if (sscanf(c->cmd, "aget %s %s", &agt_str[0], &act_str[0]) == 2) {
        // Do we have a wildcard?
        if (strncmp(&agt_str[0], "*", 1) == 0) {
            aget_beg = 0;
            aget_end = MAX_NB_OF_AGET_PER_FEMINOS - 1;
            is_valid = 1;
            is_mult = 1;
        }
        // Do we have a range specified?
        else if (sscanf(&agt_str[0], "%i:%i", &aget_beg, &aget_end) == 2) {
            // Swap arguments if not in order
            if (aget_beg > aget_end) {
                aget_tmp = aget_beg;
                aget_beg = aget_end;
                aget_end = aget_tmp;
            }
            // Check boundaries
            if ((aget_beg > (MAX_NB_OF_AGET_PER_FEMINOS - 1)) || (aget_end > (MAX_NB_OF_AGET_PER_FEMINOS - 1))) {
                is_valid = 0;
                is_mult = 1;
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) illegal aget range: %d:%d\n", err, c->fem->card_id, aget_beg, aget_end);
            } else {
                is_valid = 1;
                is_mult = 1;
            }
        }
        // Do we have a single AGET specified?
        else if (sscanf(&agt_str[0], "%i", &aget_beg) == 1) {
            aget_end = aget_beg;
            // Check boundaries
            if (aget_beg > (MAX_NB_OF_AGET_PER_FEMINOS - 1)) {
                is_valid = 0;
                is_mult = 0;
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) illegal aget index: %d\n", err, c->fem->card_id, aget_beg);
            } else {
                is_valid = 1;
                is_mult = 0;
            }
        }
        // The AGET target is invalid
        else {
            is_valid = 0;
            is_mult = 0;
            err = ERR_ILLEGAL_PARAMETER;
            sprintf(rep, "%d Fem(%02d) illegal aget target: %s\n", err, c->fem->card_id, agt_str);
        }
    } else {
        is_valid = 0;
        is_mult = 0;
        err = ERR_SYNTAX;
        sprintf(rep, "%d Fem(%02d) usage: aget <*|first:last|id> <action> <arguments>\n", err, c->fem->card_id);
    }

    // Return if any error found at this level
    if (is_valid == 0) {
        c->do_reply = 1;
        return (err);
    }

    // Read AGET slow control Register
    if (strncmp(&act_str[0], "read", 4) == 0) {
        // Only supported for single AGET
        if (is_mult == 1) {
            err = ERR_ILLEGAL_PARAMETER;
            sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                    err,
                    c->fem->card_id);
        } else {
            if (sscanf(c->cmd, "aget %s read %i", &agt_str[0], &param[0]) == 2) {
                // Read AGET # register # (16-bit to 128-bit depending on register address)
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg);
                } else {
                    // Printout data for 16-bit Registers
                    if ((param[0] == 5) || (param[0] == 12)) {
                        sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) read_value: 0x%04x\n",
                                c->fem->card_id,
                                aget_beg,
                                param[0],
                                val_rd[0]);
                    }
                    // Printout data for 32-bit Registers
                    else if ((param[0] == 1) || (param[0] == 2)) {
                        sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) read_value: 0x%04x 0x%04x\n",
                                c->fem->card_id,
                                aget_beg,
                                param[0],
                                val_rd[0],
                                val_rd[1]);
                    }
                    // Printout data for 34-bit Registers
                    else if ((param[0] == 3) || (param[0] == 4)) {
                        sprintf(rep, "0 Fem(%02d)Aget(%d) Reg(%d) read_value: 0x%04x 0x%04x 0x%04x\n",
                                c->fem->card_id,
                                aget_beg,
                                param[0],
                                val_rd[0],
                                val_rd[1],
                                val_rd[2]);
                    }
                    // Printout data for 64-bit Registers
                    else if ((param[0] == 6) || (param[0] == 7) || (param[0] == 10) || (param[0] == 11)) {
                        sprintf(rep, "0 Fem(%02d)Aget(%d) Reg(%d) read_value: 0x%04x 0x%04x 0x%04x 0x%04x\n",
                                c->fem->card_id,
                                aget_beg,
                                param[0],
                                val_rd[0],
                                val_rd[1],
                                val_rd[2],
                                val_rd[3]);
                    }
                    // Printout data for 128-bit Registers
                    else if ((param[0] == 8) || (param[0] == 9)) {
                        sprintf(rep, "0 Fem(%02d)Aget(%d) Reg(%d) read_value: 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x\n",
                                c->fem->card_id,
                                aget_beg,
                                param[0],
                                val_rd[0],
                                val_rd[1],
                                val_rd[2],
                                val_rd[3],
                                val_rd[4],
                                val_rd[5],
                                val_rd[6],
                                val_rd[7]);
                    }
                    // Illegal register address
                    else {
                        err = ERR_ILLEGAL_PARAMETER;
                        sprintf(rep, "%d Fem(%02d) Aget(%d): illegal register %d\n",
                                err,
                                c->fem->card_id,
                                aget_beg,
                                param[0]);
                    }
                }
            } else {
                err = ERR_SYNTAX;
                sprintf(rep, "%d Fem(%02d) usage: aget <id> read <reg>\n", err, c->fem->card_id);
            }
        }
    }
    // Write or Write and check AGET slow control Register
    else if ((strncmp(&act_str[0], "write", 5) == 0) || (strncmp(&act_str[0], "wrchk", 5) == 0)) {
        if (sscanf(c->cmd, "aget %s %s %i", &agt_str[0], &act_str[0], &param[0]) == 3) {
            // Get parameters for 16-bit Registers
            if ((param[0] == 5) || (param[0] == 12)) {
                if (sscanf(c->cmd, "aget %s %s %i %i", &agt_str[0], &act_str[0], &param[0], &param[1]) == 4) {
                    val_wr[0] = (unsigned short) param[1];
                    sprintf(arg_str, "0x%04x", val_wr[0]);
                } else {
                    err = ERR_SYNTAX;
                    sprintf(rep, "%d Fem(%02d) usage: aget <id> <write|wrchk> <5|12> <val_0>\n", err, c->fem->card_id);
                    c->do_reply = 1;
                    return (err);
                }
            }
            // Get parameters for 32-bit Registers
            else if ((param[0] == 1) || (param[0] == 2)) {
                if (sscanf(c->cmd, "aget %s %s %i %i %i", &agt_str[0], &act_str[0], &param[0], &param[1], &param[2]) == 5) {
                    val_wr[0] = (unsigned short) param[1];
                    val_wr[1] = (unsigned short) param[2];
                    sprintf(arg_str, "0x%04x 0x%04x", val_wr[1], val_wr[0]);
                } else {
                    err = ERR_SYNTAX;
                    sprintf(rep, "%d Fem(%02d) usage: aget <id> <write|wrchk> <1|2> <val_1> <val_0>\n", err, c->fem->card_id);
                    c->do_reply = 1;
                    return (err);
                }
            }
            // Get parameters for 34-bit Registers
            else if ((param[0] == 3) || (param[0] == 4)) {
                if (sscanf(c->cmd, "aget %s %s %i %i %i %i", &agt_str[0], &act_str[0], &param[0], &param[1], &param[2], &param[3]) == 6) {
                    val_wr[0] = (unsigned short) param[1];
                    val_wr[1] = (unsigned short) param[2];
                    val_wr[2] = (unsigned short) param[3];
                    sprintf(arg_str, "0x%04x 0x%04x 0x%04x ", val_wr[2], val_wr[1], val_wr[0]);
                } else {
                    err = ERR_SYNTAX;
                    sprintf(rep, "%d Fem(%02d) usage: aget <id> <write|wrchk> <3|4> <val_2> <val_1> <val_0>\n", err, c->fem->card_id);
                    c->do_reply = 1;
                    return (err);
                }
            }
            // Get parameters for 64-bit Registers
            else if ((param[0] == 6) || (param[0] == 7) || (param[0] == 10) || (param[0] == 11)) {
                if (sscanf(c->cmd, "aget %s %s %i %i %i %i %i", &agt_str[0], &act_str[0], &param[0], &param[1], &param[2], &param[3], &param[4]) == 7) {
                    val_wr[0] = (unsigned short) param[1];
                    val_wr[1] = (unsigned short) param[2];
                    val_wr[2] = (unsigned short) param[3];
                    val_wr[3] = (unsigned short) param[4];
                    sprintf(arg_str, "0x%04x 0x%04x 0x%04x 0x%04x ", val_wr[3], val_wr[2], val_wr[1], val_wr[0]);
                } else {
                    err = ERR_SYNTAX;
                    sprintf(rep, "%d Fem(%02d) usage: aget <id> <write|wrchk> <6|7|10|11> <val_3> <val_2> <val_1> <val_0>\n", err, c->fem->card_id);
                    c->do_reply = 1;
                    return (err);
                }
            }
            // Get parameters for 128-bit Registers
            else if ((param[0] == 8) || (param[0] == 9)) {
                if (sscanf(c->cmd, "aget %s %s %i %i %i %i %i %i %i %i %i",
                           &agt_str[0],
                           &act_str[0],
                           &param[0],
                           &param[1],
                           &param[2],
                           &param[3],
                           &param[4],
                           &param[5],
                           &param[6],
                           &param[7],
                           &param[8]) == 11) {
                    val_wr[0] = (unsigned short) param[1];
                    val_wr[1] = (unsigned short) param[2];
                    val_wr[2] = (unsigned short) param[3];
                    val_wr[3] = (unsigned short) param[4];
                    val_wr[4] = (unsigned short) param[5];
                    val_wr[5] = (unsigned short) param[6];
                    val_wr[6] = (unsigned short) param[7];
                    val_wr[7] = (unsigned short) param[8];
                    sprintf(arg_str, "0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x ",
                            val_wr[7],
                            val_wr[6],
                            val_wr[5],
                            val_wr[4],
                            val_wr[3],
                            val_wr[2],
                            val_wr[1],
                            val_wr[0]);

                } else {
                    err = ERR_SYNTAX;
                    sprintf(rep, "%d Fem(%02d) usage: aget <id> <write|wrchk> <8|9> <val_7> ... <val_0>\n", err, c->fem->card_id);
                    c->do_reply = 1;
                    return (err);
                }
            }
            // Illegal register address
            else {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget(%s): illegal register %d\n",
                        err,
                        c->fem->card_id,
                        &agt_str[0],
                        param[0]);
                c->do_reply = 1;
                return (err);
            }
            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                // Perform Write
                if (strncmp(&act_str[0], "write", 5) == 0) {
                    // Write AGET # register # (16-bit to 128-bit)
                    if ((err = Aget_Write(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                        sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Write SC register failed\n",
                                err,
                                c->fem->card_id,
                                aget_tmp,
                                param[0]);
                        c->do_reply = 1;
                        return (err);
                    }
                } else {
                    // VerifiedWrite AGET # register # (16-bit to 128-bit)
                    if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                        sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite SC register failed\n",
                                err,
                                c->fem->card_id,
                                aget_tmp,
                                param[0]);
                        c->do_reply = 1;
                        return (err);
                    }
                }
            }
            // if we reached this point, the write or verified write worked fine
            if (strncmp(&act_str[0], "write", 5) == 0) {
                sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) <- %s\n",
                        c->fem->card_id,
                        agt_str,
                        param[0],
                        arg_str);

            } else {
                sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) <- %s (verified)\n",
                        c->fem->card_id,
                        agt_str,
                        param[0],
                        arg_str);
            }
        } else {
            err = ERR_SYNTAX;
            sprintf(rep, "%d Fem(%02d) usage: aget <id> <write|wrchk> <reg> <val>\n", err, c->fem->card_id);
            c->do_reply = 1;
            return (err);
        }
    }

    // Write AGET hit Register
    else if (strncmp(&act_str[0], "wrhit", 5) == 0) {
        // Get parameters assuming 68-bit Register
        if (sscanf(c->cmd, "aget %s wrhit %i %i %i %i %i",
                   &agt_str[0],
                   &param[1],
                   &param[2],
                   &param[3],
                   &param[4],
                   &param[5]) == 6) {
            val_wr[0] = (unsigned short) param[1];
            val_wr[1] = (unsigned short) param[2];
            val_wr[2] = (unsigned short) param[3];
            val_wr[3] = (unsigned short) param[4];
            val_wr[4] = (unsigned short) param[5];
            sprintf(arg_str, "0x%04x 0x%04x 0x%04x 0x%04x 0x%04x",
                    val_wr[4],
                    val_wr[3],
                    val_wr[2],
                    val_wr[1],
                    val_wr[0]);
        } else {
            err = ERR_SYNTAX;
            sprintf(rep, "%d Fem(%02d) usage: aget <id> wrhit <val_4> <val_3> <val_2> <val_1> <val_0>\n", err, c->fem->card_id);
            c->do_reply = 1;
            return (err);
        }

        param[0] = 0; // use address 0 to specify hit register

        // Loop on selected AGET
        for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
            // Write AGET # hit register (assume 68-bit always)
            if ((err = Aget_Write(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) Aget(%d) Hit_Reg: Aget_Write failed\n",
                        err,
                        c->fem->card_id,
                        aget_tmp);
                c->do_reply = 1;
                return (err);
            }
        }
        sprintf(rep, "0 Fem(%02d) Aget(%s) Hit_reg <- %s\n",
                c->fem->card_id,
                agt_str,
                arg_str);
    }

    // Read AGET hit Register
    else if (strncmp(&act_str[0], "rdhit", 5) == 0) {
        // Only supported for single AGET
        if (is_mult == 1) {
            err = ERR_ILLEGAL_PARAMETER;
            sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                    err,
                    c->fem->card_id);
        } else {
            param[0] = 0; // use address 0 to specify hit register

            // Read AGET # hit register (assume 68-bit always)
            if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) Aget(%d) Hit_reg: Aget_Read failed\n",
                        err,
                        c->fem->card_id,
                        aget_beg);
            } else {
                sprintf(rep, "0 Fem(%02d) Aget(%d) Hit_Reg: 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x\n",
                        c->fem->card_id,
                        aget_beg,
                        val_rd[0],
                        val_rd[1],
                        val_rd[2],
                        val_rd[3],
                        val_rd[4]);
            }
        }
    }

    // Read/Write Icsa (Register 1 - bit 0)
    else if (strncmp(&act_str[0], "icsa", 4) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s icsa %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = AGET_R1S0_PUT_ICSA(c->fem->asics[aget_tmp].bar[param[0]].bars[0], (unsigned short) param[1]);
                val_wr[0] = c->fem->asics[aget_tmp].bar[param[0]].bars[1];
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Icsa <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1

                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S0_GET_ICSA(c->fem->asics[aget_beg].bar[param[0]].bars[0]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Icsa = 0x%x\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2]);
                }
            }
        }
    }

    // Read/Write Time (Register 1 - bit 3-6)
    else if (strncmp(&act_str[0], "time", 4) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s time %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = AGET_R1S0_PUT_TIME(c->fem->asics[aget_tmp].bar[param[0]].bars[0], (unsigned short) param[1]);
                val_wr[0] = c->fem->asics[aget_tmp].bar[param[0]].bars[1];
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Time <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1

                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S0_GET_TIME(c->fem->asics[aget_beg].bar[param[0]].bars[0]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Time = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_Time2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write Test (Register 1 - bit 7-8)
    else if (strncmp(&act_str[0], "test", 4) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s test %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = AGET_R1S0_PUT_TEST(c->fem->asics[aget_tmp].bar[param[0]].bars[0], (unsigned short) param[1]);
                val_wr[0] = c->fem->asics[aget_tmp].bar[param[0]].bars[1];
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Test <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1

                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S0_GET_TEST(c->fem->asics[aget_beg].bar[param[0]].bars[0]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Test = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_Test2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write mode (Register 1 - bit 14)
    else if (strncmp(&act_str[0], "mode", 4) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s mode %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = AGET_R1S0_PUT_MODE(c->fem->asics[aget_tmp].bar[param[0]].bars[0], (unsigned short) param[1]);
                val_wr[0] = c->fem->asics[aget_tmp].bar[param[0]].bars[1];
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Mode <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1
                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S0_GET_MODE(c->fem->asics[aget_beg].bar[param[0]].bars[0]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Mode = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_Mode2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write FPN readout (Register 1 - bit 15)
    else if (strncmp(&act_str[0], "fpn", 3) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s fpn %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = AGET_R1S0_PUT_FPN(c->fem->asics[aget_tmp].bar[param[0]].bars[0], (unsigned short) param[1]);
                val_wr[0] = c->fem->asics[aget_tmp].bar[param[0]].bars[1];
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) FPN_Readout <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1

                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S0_GET_FPN(c->fem->asics[aget_beg].bar[param[0]].bars[0]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x FPN_Readout = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_Fpn2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write polarity (Register 1 - bit 16)
    else if (strncmp(&act_str[0], "polarity", 8) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s polarity %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R1S1_PUT_POLARITY(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Polarity <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1

                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S1_GET_POLARITY(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Polarity = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_Polarity2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write Vicm (Register 1 - bit 17-18)
    else if (strncmp(&act_str[0], "vicm", 4) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s vicm %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R1S1_PUT_VICM(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Vicm <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1
                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S1_GET_VICM(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Vicm = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_Vicm2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write DAC (Register 1 - bit 19-22)
    else if (strncmp(&act_str[0], "dac", 3) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s dac %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R1S1_PUT_DAC(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) DAC <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1
                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S1_GET_DAC(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x DAC = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_Dac2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write Trigger veto (Register 1 - bit 23-24)
    else if (strncmp(&act_str[0], "trigger_veto", 12) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s trigger_veto %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R1S1_PUT_TVETO(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Trigger_veto <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1
                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S1_GET_TVETO(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Trigger_veto = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_TriggerVeto2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write Synchro discri (Register 1 - bit 25)
    else if (strncmp(&act_str[0], "synchro_discri", 14) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s synchro_discri %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R1S1_PUT_SDISCRI(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Synchro_discri <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1
                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S1_GET_SDISCRI(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Synchro_discri = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_SynchroDiscri2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write tot (Register 1 - bit 26)
    else if (strncmp(&act_str[0], "tot", 3) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s tot %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R1S1_PUT_TOT(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) ToT <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1
                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S1_GET_TOT(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x ToT = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_Tot2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write range trigger width (Register 1 - bit 27)
    else if (strncmp(&act_str[0], "range_tw", 8) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s range_tw %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R1S1_PUT_RANGETW(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Range_Trigger_Width <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1
                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S1_GET_RANGETW(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Range_Trigger_Width = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_RangeTw2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write trigger width (Register 1 - bit 28-29)
    else if (strncmp(&act_str[0], "trig_width", 10) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s trig_width %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 1; // act on register 1

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R1S1_PUT_TRIGW(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #1
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Trigger_Width <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 1; // act on register 1
                // Read AGET register #1
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R1S1_GET_TRIGW(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Trigger_Width = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_TrigW2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write read_from_0 (Register 2 - bit 3)
    else if (strncmp(&act_str[0], "rd_from_0", 9) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s rd_from_0 %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 2; // act on register 2

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = AGET_R2S0_PUT_READ_FROM_0(c->fem->asics[aget_tmp].bar[param[0]].bars[0], (unsigned short) param[1]);
                val_wr[0] = c->fem->asics[aget_tmp].bar[param[0]].bars[1];
                // Write AGET register #2
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Read_From_0 <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 2; // act on register 2
                // Read AGET register #2
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R2S0_GET_READ_FROM_0(c->fem->asics[aget_beg].bar[param[0]].bars[0]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Read_From_0 = 0x%x\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2]);
                }
            }
        }
    }

    // Read/Write test_digout (Register 2 - bit 4)
    else if (strncmp(&act_str[0], "tst_digout", 10) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s tst_digout %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 2; // act on register 2

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = AGET_R2S0_PUT_TEST_DIGOUT(c->fem->asics[aget_tmp].bar[param[0]].bars[0], (unsigned short) param[1]);
                val_wr[0] = c->fem->asics[aget_tmp].bar[param[0]].bars[1];
                // Write AGET register #2
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Test_Digout <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 2; // act on register 2
                // Read AGET register #2
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R2S0_GET_TEST_DIGOUT(c->fem->asics[aget_beg].bar[param[0]].bars[0]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Test_Digout = 0x%x\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2]);
                }
            }
        }
    }

    // Read/Write marker (Register 2 - bit 6)
    else if (strncmp(&act_str[0], "en_mkr_rst", 10) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s en_mkr_rst %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 2; // act on register 2

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = AGET_R2S0_PUT_EN_MKR_RST(c->fem->asics[aget_tmp].bar[param[0]].bars[0], (unsigned short) param[1]);
                val_wr[0] = c->fem->asics[aget_tmp].bar[param[0]].bars[1];
                // Write AGET register #2
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Reset_Marker_Enable <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 2; // act on register 2
                // Read AGET register #2
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R2S0_GET_EN_MKR_RST(c->fem->asics[aget_beg].bar[param[0]].bars[0]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Reset_Marker_Enable = 0x%x\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2]);
                }
            }
        }
    }

    // Read/Write marker level (Register 2 - bit 7)
    else if (strncmp(&act_str[0], "rst_level", 9) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s rst_level %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 2; // act on register 2

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = AGET_R2S0_PUT_RST_LEVEL(c->fem->asics[aget_tmp].bar[param[0]].bars[0], (unsigned short) param[1]);
                val_wr[0] = c->fem->asics[aget_tmp].bar[param[0]].bars[1];
                // Write AGET register #2
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Reset_Marker_Level <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 2; // act on register 2
                // Read AGET register #2
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R2S0_GET_RST_LEVEL(c->fem->asics[aget_beg].bar[param[0]].bars[0]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Reset_Marker_Level = 0x%x\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2]);
                }
            }
        }
    }

    // Read/Write SCA line buffer current (Register 2 - bit 13 down to 12)
    else if (strncmp(&act_str[0], "cur_ra", 6) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s cur_ra %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 2; // act on register 2

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = AGET_R2S0_PUT_CUR_RA(c->fem->asics[aget_tmp].bar[param[0]].bars[0], (unsigned short) param[1]);
                val_wr[0] = c->fem->asics[aget_tmp].bar[param[0]].bars[1];
                // Write AGET register #2
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Cur_Ra <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 2; // act on register 2
                // Read AGET register #2
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R2S0_GET_CUR_RA(c->fem->asics[aget_beg].bar[param[0]].bars[0]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Cur_Ra = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_CurRa2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write SCA line buffer current (Register 2 - bit 15 down to 14)
    else if (strncmp(&act_str[0], "cur_buf", 7) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s cur_buf %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 2; // act on register 2

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = AGET_R2S0_PUT_CUR_BUF(c->fem->asics[aget_tmp].bar[param[0]].bars[0], (unsigned short) param[1]);
                val_wr[0] = c->fem->asics[aget_tmp].bar[param[0]].bars[1];
                // Write AGET register #2
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Cur_Buf <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 2; // act on register 2
                // Read AGET register #2
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R2S0_GET_CUR_BUF(c->fem->asics[aget_beg].bar[param[0]].bars[0]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Cur_Buf = 0x%x (%s)\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2],
                            &(Aget_CurBuf2str[val_rd[2]][0]));
                }
            }
        }
    }

    // Read/Write ShortReadSeq (Register 2 - bit 19)
    else if (strncmp(&act_str[0], "short_read", 10) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s short_read %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 2; // act on register 2

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R2S1_PUT_SHORT_READ(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #2
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Short_Read <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 2; // act on register 2
                // Read AGET register #2
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R2S1_GET_SHORT_READ(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Short_Read = 0x%x\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2]);
                }
            }
        }
    }

    // Read/Write DisMultipicityOut (Register 2 - bit 20)
    else if (strncmp(&act_str[0], "dis_multiplicity_out", 20) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s dis_multiplicity_out %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 2; // act on register 2

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R2S1_PUT_DIS_MULTIPLICITY_OUT(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #2
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Dis_Multiplicity_Out <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 2; // act on register 2
                // Read AGET register #2
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R2S1_GET_DIS_MULTIPLICITY_OUT(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Dis_Multiplicity_Out = 0x%x\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2]);
                }
            }
        }
    }

    // Read/Write AutoresetBank (Register 2 - bit 21)
    else if (strncmp(&act_str[0], "autoreset_bank", 14) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s autoreset_bank %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 2; // act on register 2

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R2S1_PUT_AUTORESET_BANK(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #2
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) Autoreset_Bank <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 2; // act on register 2
                // Read AGET register #2
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R2S1_GET_AUTORESET_BANK(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x Autoreset_Bank = 0x%x\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2]);
                }
            }
        }
    }

    // Read/Write input-dynamic-range (Register 2 - bit 24)
    else if (strncmp(&act_str[0], "in_dyn_range", 12) == 0) {
        // Get parameter if write
        if (sscanf(c->cmd, "aget %s in_dyn_range %i",
                   &agt_str[0],
                   &param[1]) == 2) {
            param[0] = 2; // act on register 2

            // Loop on selected AGET
            for (aget_tmp = aget_beg; aget_tmp <= aget_end; aget_tmp++) {
                val_wr[1] = c->fem->asics[aget_tmp].bar[param[0]].bars[0];
                val_wr[0] = AGET_R2S1_PUT_IN_DYN_RANGE(c->fem->asics[aget_tmp].bar[param[0]].bars[1], (unsigned short) param[1]);
                // Write AGET register #2
                if ((err = Aget_VerifiedWrite(c->fem, (unsigned short) aget_tmp, (unsigned short) param[0], &val_wr[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_VerifiedWrite failed\n",
                            err,
                            c->fem->card_id,
                            aget_tmp,
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            }
            sprintf(rep, "0 Fem(%02d) Aget(%s) Reg(%d) In_Dyn_Range <- %d\n",
                    c->fem->card_id,
                    &agt_str[0],
                    param[0],
                    param[1]);
        } else {
            // Only supported for single AGET
            if (is_mult == 1) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) Aget range is not supported for read operations\n",
                        err,
                        c->fem->card_id);
            } else {
                param[0] = 2; // act on register 2
                // Read AGET register #2
                if ((err = Aget_Read(c->fem, (unsigned short) aget_beg, (unsigned short) param[0], &val_rd[0])) < 0) {
                    sprintf(rep, "%d Fem(%02d) Aget(%d) Reg(%d): Aget_Read failed\n",
                            err,
                            c->fem->card_id,
                            aget_beg,
                            param[0]);
                } else {
                    val_rd[2] = AGET_R2S1_GET_IN_DYN_RANGE(c->fem->asics[aget_beg].bar[param[0]].bars[1]);
                    sprintf(rep, "0 Fem(%02d) Aget(%d) Reg(%d) = 0x%04x 0x%04x In_Dyn_Range = 0x%x\n",
                            c->fem->card_id,
                            aget_beg,
                            param[0],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2]);
                }
            }
        }
    }

    // Commands to act on a per channel setting
    else if ((strncmp(&act_str[0], "gain", 4) == 0) || (strncmp(&act_str[0], "threshold", 9) == 0) || (strncmp(&act_str[0], "hitprob", 7) == 0) || (strncmp(&act_str[0], "inhibit", 7) == 0)) {
        err = Cmdi_AgetChannelCommands(c);
    }

    // Unsupported action
    else {
        err = ERR_ILLEGAL_PARAMETER;
        sprintf(rep, "%d Fem(%02d) Aget(%s) : %s illegal operation\n",
                err,
                c->fem->card_id,
                &agt_str[0],
                &act_str[0]);
    }

    c->do_reply = 1;

    return (err);
}
