/*******************************************************************************
                           Minos / Feminos
                           _______________

 File:        cmdi.c

 Description: A command interpreter to control one Feminos.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr

 History:
  May 2011: created

  June 2012: changed format of daq command to add unit B (bytes) or F (frames)

  September 2012: added code to copy the socket address information of the
  sender of the command.

  October 2012: added I2C commands to read I2C devices on Mars MX2 module

  October 2012: added an optional sequence number at the end of daq command
  to be able to detect losses of daq requests.
  Updated command statistics frame to include counter of missing daq requests.

  October 2012: added Flash commands to read / write pages in the external
  SPI Flash to allow firmware/software upgrade via Ethernet instead of JTAG

  October 2012: added call to SockUDP_RemoveBufferFromPendingAckStack() when
  a daq command is received to release the buffer that was pending acknowledge
  from the DAQ (only done when the packet loss policy is "re-send")

  November 2012: optimized daq command. Changed the way ASCII response buffers
  are filled. We now reserved enough space for the Ethernet + IP + UDP +
  optional protocol in the buffer before the start of data. This avoids the
  need to get another buffer for the header and send frames composed of two
  descriptors.

  June 2013: added command "tstamp_init" to set initial value of time-stamp
  upon synchronous reset (now becomes preset although name is unchanged).

  January 2014: corrected error on calculation of datagram size that prevented
  END_OF_FRAME to be sent

  January 2014: now print 2 digits for Fem index in responses so that printout
  are aligned

  February 2014: added commands dcbal_enc, dcbal_dec and pul_enable, mult_thr,
  mult_trig_ena

  March 2014: added function Cmdi_ActOnPolarity() to implement a "polarity"
  command that supports ASIC parameter (wilcard, range or ASIC number)

  June 2014: added command "tstamp_isset", "tstamp_isset clr", "snd_mult_ena"

  June 2014: added command "tcm_bert" and "mult_limit"

  June 2014: added command "erase_hit_ena" and "erase_hit_thr"
  Renamed Cmdi_ActOnPolarity() into Cmdi_ActOnSettings(). This now implements
  the following commands: "polarity", "mult_thr", "mult_limit", "erase_hit_thr"

*******************************************************************************/

#include "cmdi.h"
#include "after.h"
#include "aget.h"
#include "busymeter.h"
#include "cmdi_after.h"
#include "cmdi_aget.h"
#include "cmdi_flash.h"
#include "cmdi_i2c.h"
#include "cmdi_strings.h"
#include "ds2438_api.h"
#include "error_codes.h"
#include "finetime.h"
#include "frame.h"
#include "hit_histo.h"
#include "hitscurve.h"
#include "minibios.h"
#include "pedestal.h"
#include "pulser.h"

#include <stdio.h>
#include <string.h>

#define TEST_DATA_TABLE_SZ 4096

PedThr __attribute__((section(".pedthrlut"))) pedthrlut[MAX_NB_OF_ASIC_PER_FEMINOS][ASIC_CHAN_TABLE_SZ];

typedef struct _HitRegRule {
    unsigned short forceon;
    unsigned short forceoff;
} HitRegRule;

HitRegRule __attribute__((section(".hitregrule"))) hitregrule[MAX_NB_OF_ASIC_PER_FEMINOS][ASIC_CHAN_TABLE_SZ];

unsigned int __attribute__((section(".testdata"))) testdata[TEST_DATA_TABLE_SZ];

////////////////////////////////////////////////
// Pick compilation Date and Time
char server_date[] = __DATE__;
char server_time[] = __TIME__;
////////////////////////////////////////////////

/*******************************************************************************
 Useful macro to convert an hexadecimal character to an hexadecimal digit
*******************************************************************************/
#define ASCII2HEX(c, uc)                            \
    {                                               \
        if ((c >= '0') && (c <= '9')) uc = c - '0'; \
        else if ((c >= 'a') && (c <= 'f'))          \
            uc = c - 'a' + 0xA;                     \
        else if ((c >= 'A') && (c <= 'F'))          \
            uc = c - 'A' + 0xA;                     \
        else                                        \
            uc = 0;                                 \
    }

/*******************************************************************************
 trig_range_rate2freq - converts trigger range and rate to frequency
*******************************************************************************/
float trig_range_rate2freq(unsigned int range, unsigned int rate) {
    if (range == 0) {
        return (0.1 * (float) rate);
    } else if (range == 1) {
        return (10 * (float) rate);
    } else if (range == 2) {
        return (100 * (float) rate);
    } else if (range == 3) {
        return (1000 * (float) rate);
    } else {
        return (0.0);
    }
}

/*******************************************************************************
 Cmdi_ScaCommands
*******************************************************************************/
int Cmdi_ScaCommands(CmdiContext* c) {
    unsigned int param[10];
    unsigned int val_rdi;
    char* rep;
    int err, err2;
    float fwrite;

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME

    // Command to make software trigger
    if (strncmp(c->cmd + 4, "stop", 4) == 0) {
        // Set SCA_STOP to 1
        param[0] = R0_SCA_STOP;
        if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_SCA_STOP, &param[0])) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
        }
        // Set SCA_STOP to 0
        param[0] = 0;
        if ((err2 = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_SCA_STOP, &param[0])) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
        }
        if ((err == 0) && (err2 == 0)) {
            sprintf(rep, "0 Fem(%02d) pulsed sca_stop\n", c->fem->card_id);
        }
        if (err2 < 0) {
            err = err2;
        }
    }

    // Command to start writing in SCA in software
    else if (strncmp(c->cmd + 4, "start", 5) == 0) {
        // Set SCA_START to 1
        param[0] = R0_SCA_START;
        if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_SCA_START, &param[0])) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
        }
        // Set SCA_START to 0
        param[0] = 0;
        if ((err2 = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_SCA_START, &param[0])) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
        }
        if ((err == 0) && (err2 == 0)) {
            sprintf(rep, "0 Fem(%02d) pulsed sca_start\n", c->fem->card_id);
        }
        if (err2 < 0) {
            err = err2;
        }
    }

    // Command to show/set SCA cell count
    else if (strncmp(c->cmd + 4, "cnt", 3) == 0) {
        if (sscanf(c->cmd + 4, "cnt %i", &param[0]) == 1) {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_SCA_CNT_M, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, R0_SCA_CNT_M, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d)\n", c->fem->card_id, 0, val_rdi, val_rdi);
            }
        }
    }

    // Command to show/set SCA write clock divisor
    else if (strncmp(c->cmd + 4, "wckdiv", 6) == 0) {
        if (sscanf(c->cmd + 4, "wckdiv %i", &param[0]) == 1) {
            param[0] = R5_PUT_WCKDIV(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 5, R5_WCKDIV_M, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 5, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 5, R5_WCKDIV_M, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                if (R5_GET_WCKDIV(val_rdi) != 0) {
                    fwrite = 100.0 / (float) (R5_GET_WCKDIV(val_rdi));
                } else {
                    fwrite = 0.0;
                }
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) WckDiv: %d (%.2f MHz)\n",
                        c->fem->card_id,
                        5,
                        val_rdi,
                        val_rdi,
                        R5_GET_WCKDIV(val_rdi),
                        fwrite);
            }
        }
    }

    // Command to enable/disable SCA operation
    else if (strncmp(c->cmd + 4, "enable", 6) == 0) {
        if (sscanf(c->cmd + 4, "enable %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R0_SCA_ENABLE;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_SCA_ENABLE, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Sca_enable: %d\n",
                        c->fem->card_id,
                        0,
                        val_rdi,
                        val_rdi,
                        R0_GET_SCA_ENABLE(val_rdi));
            }
        }
    }
    // Command to enable/disable SCA auto-start operation
    else if (strncmp(c->cmd + 4, "autostart", 9) == 0) {
        if (sscanf(c->cmd + 4, "autostart %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R0_SCA_AUTO_START;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_SCA_AUTO_START, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Sca_auto_start: %d\n",
                        c->fem->card_id,
                        0,
                        val_rdi,
                        val_rdi,
                        R0_GET_SCA_AUTO_START(val_rdi));
            }
        }
    }

    // Syntax error or unsupported command
    else {
        err = -1;
        sprintf(rep, "%d Fem(%02d): sca [cnt|wckdiv|enable|autostart|stop]\n", err, c->fem->card_id);
    }

    c->do_reply = 1;
    return (err);
}

/*******************************************************************************
 Cmdi_ActOnThrPedHrule
*******************************************************************************/
int Cmdi_ActOnThrPedHrule(CmdiContext* c) {
    int arg_cnt;
    int is_mult;
    int act_cnt;
    int err;
    unsigned short asic_cur, asic_beg, asic_end;
    unsigned short cha_cur, cha_beg, cha_end;
    char str[8][16];
    int param[10];
    unsigned short val_rds;
    unsigned short val_wrs;
    unsigned short lut_base;
    char* rep;
    char name[16];

    // Init
    err = 0;
    asic_beg = 0;
    asic_end = 0;
    cha_beg = 0;
    cha_end = 0;
    is_mult = 0;

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME
    c->do_reply = 1;

    // Scan arguments
    if ((arg_cnt = sscanf(c->cmd, "%s %s %s %s", &str[0][0], &str[1][0], &str[2][0], &str[3][0])) >= 3) {
        // Wildcard on ASIC?
        if (str[1][0] == '*') {
            asic_beg = 0;
            asic_end = MAX_NB_OF_ASIC_PER_FEMINOS - 1;
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
        if (str[2][0] == '*') {
            cha_beg = 0;
            cha_end = ASIC_MAX_CHAN_INDEX;
            is_mult = 1;
        } else {
            // Scan Channel index range
            if (sscanf(&str[2][0], "%i:%i", &param[0], &param[1]) == 2) {
                cha_beg = (unsigned short) param[0];
                cha_end = (unsigned short) param[1];
                is_mult = 1;
            }
            // Scan Channel index
            else if (sscanf(&str[2][0], "%i", &param[0]) == 1) {
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
    if (arg_cnt == 4) {
        if (sscanf(&str[3][0], "%i", &param[3]) != 1) {
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
        (asic_end >= MAX_NB_OF_ASIC_PER_FEMINOS) ||
        (cha_end > ASIC_MAX_CHAN_INDEX) ||
        ((arg_cnt == 3) && (is_mult == 1))) {
        err = -1;
        sprintf(rep, "%d Fem(%02d) %s <ASIC> <Channel> <0xValue>\n", err, c->fem->card_id, &str[0][0]);
        return (err);
    }

    // Set the offset address to act upon
    if (strncmp(&str[0][0], "ped", 3) == 0) {
        lut_base = 0;
    } else if (strncmp(&str[0][0], "thr", 3) == 0) {
        lut_base = 1;
    } else if (strncmp(&str[0][0], "forceon", 7) == 0) {
        lut_base = 2;
    } else if (strncmp(&str[0][0], "forceoff", 8) == 0) {
        lut_base = 3;
    } else {
        sprintf(rep, "-1 Fem(%02d) illegal command %s", c->fem->card_id, &str[0][0]);
        return (-1);
    }

    // Read single entry
    if (arg_cnt == 3) {
        if (lut_base == 0) {
            val_rds = pedthrlut[asic_beg][cha_beg].ped;
            sprintf(rep, "0 Fem(%02d) ped[%d][%d] = 0x%x (%d)\n", c->fem->card_id, asic_beg, cha_beg, val_rds, val_rds);
        } else if (lut_base == 1) {
            val_rds = pedthrlut[asic_beg][cha_beg].thr;
            sprintf(rep, "0 Fem(%02d) thr[%d][%d] = 0x%x (%d)\n", c->fem->card_id, asic_beg, cha_beg, val_rds, val_rds);
        } else if (lut_base == 2) {
            val_rds = hitregrule[asic_beg][cha_beg].forceon;
            sprintf(rep, "0 Fem(%02d) forceon[%d][%d] = 0x%x\n", c->fem->card_id, asic_beg, cha_beg, val_rds);
        } else if (lut_base == 3) {
            val_rds = hitregrule[asic_beg][cha_beg].forceoff;
            sprintf(rep, "0 Fem(%02d) forceoff[%d][%d] = 0x%x\n", c->fem->card_id, asic_beg, cha_beg, val_rds);
        }
        return (0);
    }

    // Write multiple pedestal or threshold entries
    val_wrs = (unsigned short) param[3];
    act_cnt = 0;
    for (asic_cur = asic_beg; asic_cur <= asic_end; asic_cur++) {
        for (cha_cur = cha_beg; cha_cur <= cha_end; cha_cur++) {
            if (lut_base == 0) {
                pedthrlut[asic_cur][cha_cur].ped = val_wrs;
            } else if (lut_base == 1) {
                pedthrlut[asic_cur][cha_cur].thr = val_wrs;
            } else if (lut_base == 2) {
                hitregrule[asic_cur][cha_cur].forceon = val_wrs;
            } else if (lut_base == 3) {
                hitregrule[asic_cur][cha_cur].forceoff = val_wrs;
            }
            act_cnt++;
        }
    }

    // Make result string
    if (lut_base == 0) {
        sprintf(name, "ped");
    } else if (lut_base == 1) {
        sprintf(name, "thr");
    } else if (lut_base == 2) {
        sprintf(name, "forceon");
    } else if (lut_base == 3) {
        sprintf(name, "forceoff");
    }

    if (is_mult == 1) {
        sprintf(rep, "0 Fem(%02d) %s[%d:%d][%d:%d]= 0x%x (%d) (wrote %d entries)\n", c->fem->card_id, name, asic_beg, asic_end, cha_beg, cha_end, val_wrs, val_wrs, act_cnt);
    } else {
        sprintf(rep, "0 Fem(%02d) %s[%d][%d]= 0x%x (%d) (wrote %d entries)\n", c->fem->card_id, name, asic_beg, cha_beg, val_wrs, val_wrs, act_cnt);
    }
    return (0);
}

/*******************************************************************************
 Cmdi_ActOnSettings
*******************************************************************************/
int Cmdi_ActOnSettings(CmdiContext* c) {
    int arg_cnt;
    int is_mult;
    int act_cnt;
    int err;
    unsigned short asic_cur, asic_beg, asic_end;
    char str[4][16];
    unsigned int param[10];
    unsigned int val_rdi;
    char* rep;

    // Init
    err = 0;
    asic_beg = 0;
    asic_end = 0;
    is_mult = 0;

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME
    c->do_reply = 1;

    // Scan arguments
    if ((arg_cnt = sscanf(c->cmd, "%s %s %s", &str[0][0], &str[1][0], &str[2][0])) >= 2) {
        // Command polarity
        if (strncmp(&str[0][0], "polarity", 8) == 0) {
            param[3] = 0; // polarity
            param[4] = 3; // act on Register 3
            sprintf(&str[3][0], "Polarity");
        }
        // Command polarity
        else if (strncmp(&str[0][0], "erase_hit_thr", 13) == 0) {
            param[3] = 1; // erase_hit_thr
            param[4] = 3; // act on Register 3
            sprintf(&str[3][0], "Erase_Hit_Thr");
        }
        // Command mult_thr
        else if (strncmp(&str[0][0], "mult_thr", 8) == 0) {
            param[3] = 2; // mult_thr
            param[4] = 4; // act on Register 4
            sprintf(&str[3][0], "Mult_Thr");
        }
        // Command mult_limit
        else if (strncmp(&str[0][0], "mult_limit", 10) == 0) {
            param[3] = 3; // mult_limit
            param[4] = 8; // act on Register 8
            sprintf(&str[3][0], "Mult_Limit");
        }
        // Unknown command
        else {
            err = -1;
            sprintf(rep, "%d Fem(%02d) unsupported command %s\n", err, c->fem->card_id, c->cmd);
            return (err);
        }

        // Wildcard on ASIC?
        if (str[1][0] == '*') {
            asic_beg = 0;
            asic_end = MAX_NB_OF_ASIC_PER_FEMINOS - 1;
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
    } else {
        err = -1;
        sprintf(rep, "%d Fem(%02d) missing parameter in %s\n", err, c->fem->card_id, c->cmd);
        return (err);
    }

    // Scan arguments for write operation
    if (arg_cnt == 3) {
        if (sscanf(&str[2][0], "%i", &param[2]) != 1) {
            sprintf(rep, "%d Fem(%02d) illegal parameter in %s\n", err, c->fem->card_id, c->cmd);
            err = -1;
            return (err);
        }
    }

    // Swap begin and end arguments if not in order
    if (asic_beg > asic_end) {
        asic_cur = asic_end;
        asic_end = asic_beg;
        asic_beg = asic_cur;
    }

    // Check that supplied arguments are within acceptable range
    if ((asic_end >= MAX_NB_OF_ASIC_PER_FEMINOS) || ((arg_cnt == 2) && (is_mult == 1))) {
        err = -1;
        sprintf(rep, "%d Fem(%02d) syntax: %s <ASIC> <0xValue>\n", err, c->fem->card_id, str[0]);
        return (err);
    }

    // Read single entry
    if (arg_cnt == 2) {
        if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, param[4], 0xFFFFFFFF, &val_rdi)) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
        } else {
            // polarity
            if (param[3] == 0) {
                param[5] = R3_GET_POLARITY(val_rdi, asic_beg);
            }
            // erase_hit_thr
            else if (param[3] == 1) {
                param[5] = R3_GET_ERASE_HIT_THR(val_rdi, asic_beg);
            }
            // mult_thr
            else if (param[3] == 2) {
                param[5] = R4_GET_MULT_THR(val_rdi, asic_beg);
            }
            // mult_limit
            else if (param[3] == 3) {
                param[5] = R8_GET_MULT_LIMIT(val_rdi, asic_beg);
            } else {
                // Should never reach here
                err = -1;
                sprintf(rep, "%d Fem(%02d): Internal error - Syndrome: param[3] = %d\n", err, c->fem->card_id, param[3]);
            }

            sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) %s(%d): 0x%x (%d)\n",
                    c->fem->card_id,
                    param[4],
                    val_rdi,
                    val_rdi,
                    str[3],
                    asic_beg,
                    param[5],
                    param[5]);
        }
        return (err);
    }

    // Write one or multiple entries
    act_cnt = 0;
    for (asic_cur = asic_beg; asic_cur <= asic_end; asic_cur++) {
        // polarity
        if (param[3] == 0) {
            param[5] = (R3_POLARITY_0 << asic_cur);
            param[6] = R3_PUT_POLARITY(0, asic_cur, param[2]);
        }
        // erase_hit_thr
        else if (param[3] == 1) {
            param[5] = R3_PUT_ERASE_HIT_THR_M(0, asic_cur);
            param[6] = R3_PUT_ERASE_HIT_THR(0, asic_cur, param[2]);
        }
        // mult_thr
        else if (param[3] == 2) {
            param[5] = R4_PUT_MULT_THR_M(0, asic_cur);
            param[6] = R4_PUT_MULT_THR(0, asic_cur, param[2]);
        }
        // mult_limit
        else if (param[3] == 3) {
            param[5] = R8_PUT_MULT_LIMIT_M(0, asic_cur);
            param[6] = R8_PUT_MULT_LIMIT(0, asic_cur, param[2]);
        } else {
            // Should never reach here
            err = -1;
            sprintf(rep, "%d Fem(%02d): Internal error - Syndrome: param[3] = %d\n", err, c->fem->card_id, param[3]);
            return (err);
        }

        if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, param[4], param[5], &param[6])) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            return (err);
        } else {
            act_cnt++;
            // polarity
            if (param[3] == 0) {
                c->polarity[asic_cur] = param[2];
            }
            // erase_hit_thr
            else if (param[3] == 1) {
                // nothing to do
            }
            // mult_thr
            else if (param[3] == 2) {
                // nothing to do
            }
            // mult_limit
            else if (param[3] == 3) {
                // nothing to do
            } else {
                // Should never reach here
                err = -1;
                sprintf(rep, "%d Fem(%02d): Internal error - Syndrome: param[3] = %d\n", err, c->fem->card_id, param[3]);
                return (err);
            }
        }
    }
    sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x (performed %d actions)\n", c->fem->card_id, 3, param[6], act_cnt);

    return (err);
}

/*******************************************************************************
 Cmdi_ActOnTestData
*******************************************************************************/
int Cmdi_ActOnTestData(CmdiContext* c) {
    int param[10];
    int err;
    char* rep;
    int i;
    char str[10];

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME

    if ((err = sscanf(c->cmd, "tdata %d %i", &param[0], &param[1])) >= 1) {
        // Read test data
        if (err == 1) {
            param[0] = param[0] & 0xFFF;
            sprintf(rep, "0 Fem(%02d) TestData(0x%04x) = 0x%x (%d)\n", c->fem->card_id, param[0], testdata[param[0]], testdata[param[0]]);
        }
        // Write test data
        else if (err == 2) {
            param[0] = param[0] & 0xFFF;
            testdata[param[0]] = param[1] & 0xFFF;
            sprintf(rep, "0 Fem(%02d) TestData(0x%04x) <- 0x%x (%d)\n", c->fem->card_id, param[0], param[1], param[1]);
        }
        err = 0;
    } else if ((err = sscanf(c->cmd, "tdata %c %i", &str[0], &param[0])) == 2) {
        // Test pattern A: linear increasing ramp
        if (str[0] == 'A') {
            for (i = 0; i < TEST_DATA_TABLE_SZ; i++) {
                testdata[i] = i % param[0];
            }
            sprintf(rep, "0 Fem(%02d) TestData: linear ramp from 0 to %d\n", c->fem->card_id, (param[0] - 1));
            err = 0;
        }
        // Test pattern B: linear ramp decreasing ramp
        else if (str[0] == 'B') {
            for (i = 0; i < TEST_DATA_TABLE_SZ; i++) {
                testdata[i] = (param[0] - 1) - (i % param[0]);
            }
            sprintf(rep, "0 Fem(%02d) TestData: linear ramp from %d to 0\n", c->fem->card_id, (param[0] - 1));
            err = 0;
        }
        // Test pattern C: constant pulse every N
        else if (str[0] == 'C') {
            for (i = 0; i < TEST_DATA_TABLE_SZ; i++) {
                if ((i % param[0]) == (param[0] - 1)) {
                    testdata[i] = 2048;
                } else {
                    testdata[i] = 0;
                }
            }
            sprintf(rep, "0 Fem(%02d) TestData: pulse every %d time bins\n", c->fem->card_id, param[0]);
            err = 0;
        } else {
            err = -1;
            sprintf(rep, "%d Fem(%02d) syntax: tdata <T> <0xData>\n", err, c->fem->card_id);
        }
    } else {
        err = -1;
        sprintf(rep, "%d Fem(%02d) syntax: tdata <0xAdr> [<0xData>]\n", err, c->fem->card_id);
    }

    c->do_reply = 1;

    return (err);
}

/*******************************************************************************
 Cmdi_ListCommand
*******************************************************************************/
int Cmdi_ListCommand(CmdiContext* c) {
    int param[10];
    int err;
    char* rep;
    char str[2][16];
    unsigned short asic_cur, asic_beg, asic_end;
    unsigned short cha_cur, cha_beg, cha_end;
    int lut_base;
    unsigned short size;
    unsigned short* p;

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME

    // Init
    asic_beg = 0;
    asic_end = 0;
    cha_beg = 0;
    cha_end = 0;
    lut_base = 0;
    err = 0;

    if ((param[0] = sscanf(c->cmd, "list %s %s", &str[0][0], &str[1][0])) != 2) {
        err = -1;
        sprintf(rep, "%d Fem(%02d) list: missing argument (%d instead of 2)\n", err, c->fem->card_id, param[0]);
        c->do_reply = 1;
        return (err);
    }
    // Scan ASIC wildcard
    else if (strncmp(&str[1][0], "*", 1) == 0) {
        asic_beg = 0;
        asic_end = MAX_NB_OF_ASIC_PER_FEMINOS - 1;
    }
    // Scan ASIC index range
    else if (sscanf(&str[1][0], "%i:%i", &param[0], &param[1]) == 2) {
        if (param[0] < param[1]) {
            asic_beg = (unsigned short) param[0];
            asic_end = (unsigned short) param[1];
        } else {
            asic_beg = (unsigned short) param[1];
            asic_end = (unsigned short) param[0];
        }
    }
    // Scan ASIC index
    else if (sscanf(&str[1][0], "%i", &param[0]) == 1) {
        asic_beg = (unsigned short) param[0];
        asic_end = (unsigned short) param[0];
    } else {
        err = -1;
        sprintf(rep, "%d Fem(%02d) syntax: list <ped | thr> <asic>\n", err, c->fem->card_id);
        c->do_reply = 1;
        return (err);
    }

    // Do we want pedestals
    if (strncmp(&str[0][0], "ped", 3) == 0) {
        lut_base = 0;
    }
    // or do we want thresholds
    else if (strncmp(&str[0][0], "thr", 3) == 0) {
        lut_base = 1;
    }
    // Scanning arguments failed
    else {
        err = -1;
        sprintf(rep, "%d Fem(%02d) syntax: list <ped | thr> <asic>\n", err, c->fem->card_id);
        c->do_reply = 1;
        return (err);
    }

    // Determine the number of entries to read
    if (c->mode == 0) // AGET
    {
        cha_end = MAX_CHAN_INDEX_AGET;
    } else // AFTER
    {
        cha_end = MAX_CHAN_INDEX_AFTER;
    }

    // Now prepare the response frame
    size = 0;
    p = (unsigned short*) (c->burep);
    c->reply_is_cframe = 0;

    // Put Start Of Frame + Version
    *p = PUT_FVERSION_FEMID(PFX_START_OF_MFRAME, CURRENT_FRAMING_VERSION, c->fem->card_id);
    p++;
    size += 2;

    // Leave space for size;
    p++;
    size += sizeof(unsigned short);

    // Read multiple pedestal or threshold entries
    for (asic_cur = asic_beg; asic_cur <= asic_end; asic_cur++) {
        // Put Card/Chip/Mode for List
        *p = PUT_PEDTHR_LIST(c->fem->card_id, asic_cur, c->mode, lut_base);
        p++;
        size += sizeof(unsigned short);

        // List the pedestal or threshold for all the channels
        for (cha_cur = cha_beg; cha_cur <= cha_end; cha_cur++) {
            if (lut_base == 0) {
                *p = pedthrlut[asic_cur][cha_cur].ped;
            } else if (lut_base == 1) {
                *p = pedthrlut[asic_cur][cha_cur].thr;
            }
            p++;
            size += sizeof(unsigned short);
        }
    }

    // Put End Of Frame
    *p = PFX_END_OF_FRAME;
    p++;
    size += sizeof(unsigned short);

    // Put payload size after start of frame
    p = (unsigned short*) (c->burep);
    p++;
    *p = size;

    // Include in the size the 2 empty bytes kept at the beginning of the UDP datagram
    size += 2;
    c->rep_size = size;
    c->do_reply = 1;

    return (err);
}

/*******************************************************************************
 Cmdi_CmdCommands
*******************************************************************************/
int Cmdi_CmdCommands(CmdiContext* c) {
    int err;
    char* rep;
    unsigned short size;
    unsigned short* p;

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME
    err = 0;

    // Command to clear command statistics
    if (strncmp(c->cmd, "cmd clr", 7) == 0) {
        // We cannot completely clear the rx_cmd_cnt because we also count
        // the clear command itself when it is received
        c->rx_cmd_cnt = 1;
        c->rx_daq_cnt = 0;
        c->rx_daq_timeout = 0;
        c->rx_daq_delayed = 0;
        c->err_cmd_cnt = 0;
        c->tx_rep_cnt = 0;
        c->tx_daq_cnt = 0;
        c->daq_miss_cnt = 0;

        sprintf(rep, "0 Fem(%02d) command statistics cleared\n", c->fem->card_id);
    }
    // Put statistics
    else if (strncmp(c->cmd, "cmd stat", 8) == 0) {
        // Build response frame
        size = 0;
        p = (unsigned short*) (c->burep);
        c->reply_is_cframe = 0;

        // Put Start Of Frame + Version
        *p = PUT_FVERSION_FEMID(PFX_START_OF_MFRAME, CURRENT_FRAMING_VERSION, c->fem->card_id);
        p++;
        size += 2;

        // Leave space for size;
        p++;
        size += sizeof(unsigned short);

        // Put Command statistics prefix
        *p = PFX_CMD_STATISTICS;
        p++;
        size += sizeof(unsigned short);

        // Put Command statistics
        *p = (unsigned short) (c->rx_cmd_cnt & 0xFFFF);
        p++;
        *p = (unsigned short) ((c->rx_cmd_cnt & 0xFFFF0000) >> 16);
        p++;
        size += sizeof(int);
        *p = (unsigned short) (c->rx_daq_cnt & 0xFFFF);
        p++;
        *p = (unsigned short) ((c->rx_daq_cnt & 0xFFFF0000) >> 16);
        p++;
        size += sizeof(int);
        *p = (unsigned short) (c->rx_daq_timeout & 0xFFFF);
        p++;
        *p = (unsigned short) ((c->rx_daq_timeout & 0xFFFF0000) >> 16);
        p++;
        size += sizeof(int);
        *p = (unsigned short) (c->rx_daq_delayed & 0xFFFF);
        p++;
        *p = (unsigned short) ((c->rx_daq_delayed & 0xFFFF0000) >> 16);
        p++;
        size += sizeof(int);
        *p = (unsigned short) (c->daq_miss_cnt & 0xFFFF);
        p++;
        *p = (unsigned short) ((c->daq_miss_cnt & 0xFFFF0000) >> 16);
        p++;
        size += sizeof(int);
        *p = (unsigned short) (c->err_cmd_cnt & 0xFFFF);
        p++;
        *p = (unsigned short) ((c->err_cmd_cnt & 0xFFFF0000) >> 16);
        p++;
        size += sizeof(int);
        // We anticipate on the next value of tx_rep_cnt to take into account the
        // fact that when we fill the present data packet, we do not know yet if it
        // will be sent successfully. The problem is that if we increment tx_rep_cnt
        // after the packet has been really sent, the number of commands replies
        // is greater by one unit compared to the number of command received - which
        // is normal but non-intuitive.
        *p = (unsigned short) ((c->tx_rep_cnt + 1) & 0xFFFF);
        p++;
        *p = (unsigned short) ((c->tx_rep_cnt & 0xFFFF0000) >> 16);
        p++;
        size += sizeof(int);
        *p = (unsigned short) (c->tx_daq_cnt & 0xFFFF);
        p++;
        *p = (unsigned short) ((c->tx_daq_cnt & 0xFFFF0000) >> 16);
        p++;
        size += sizeof(int);
        *p = (unsigned short) (c->tx_daq_resend_cnt & 0xFFFF);
        p++;
        *p = (unsigned short) ((c->tx_daq_resend_cnt & 0xFFFF0000) >> 16);
        p++;
        size += sizeof(int);

        // Put End Of Frame
        *p = PFX_END_OF_FRAME;
        p++;
        size += sizeof(unsigned short);

        // Put packet size after start of frame
        p = (unsigned short*) (c->burep);
        p++;
        *p = size;

        // Include in the size the 2 empty bytes kept at the beginning of the UDP datagram
        size += 2;
        c->rep_size = size;
    } else {
        err = -1;
        sprintf(rep, "%d Fem(%02d) syntax: cmd <clr | stat>\n", err, c->fem->card_id);
    }

    c->do_reply = 1;
    return (err);
}

/*******************************************************************************
 Cmdi_Cmd_Usage
  fill a string with command syntax outline
*******************************************************************************/
void Cmdi_Cmd_Usage(char* r, int sz) {
    unsigned int r_capa = sz - 8;

    sprintf(r, "0 Commands:\n");
    if (strlen(r) < r_capa) strcat(r, "help verbose version\n");
    if (strlen(r) < r_capa) strcat(r, "ped thr\n");
    if (strlen(r) < r_capa) strcat(r, "reg\n");
    if (strlen(r) < r_capa) strcat(r, "sca cnt/wckdiv/enable/autostart/stop\n");
    if (strlen(r) < r_capa) strcat(r, "mode after/aget\n");
    if (strlen(r) < r_capa) strcat(r, "emit_hit_cnt rst_len forceon_all keep_rst skip_rst\n");
}

/*******************************************************************************
 Cmdi_Context_Init
*******************************************************************************/
void Cmdi_Context_Init(CmdiContext* c) {
    int i;

    c->snd_allowed = 0;
    c->request_unit = 'B'; // Default request unit is bytes
    c->frrep = (void*) 0;
    c->burep = (void*) 0;
    c->reply_is_cframe = 0;
    c->rep_size = 0;
    c->do_reply = 0;
    c->bp = (BufPool*) 0;
    c->et = (Ethernet*) 0;
    c->lst_socket = -1;
    c->daq_socket = -1;
    c->ar = (AxiRingBuffer*) 0;
    c->i2c_inst = (XIic*) 0;
    c->mode = 0;
    for (i = 0; i < MAX_NB_OF_ASIC_PER_FEMINOS; i++) {
        c->polarity[i] = 0;
    }
    c->serve_target = RBF_DATA_TARGET_DAQ;
    c->rx_cmd_cnt = 0;
    c->rx_daq_cnt = 0;
    c->rx_daq_timeout = 0;
    c->rx_daq_delayed = 0;
    c->err_cmd_cnt = 0;
    c->tx_rep_cnt = 0;
    c->tx_daq_cnt = 0;
    c->tx_daq_resend_cnt = 0;
    c->loss_policy = 0;
    c->cred_wait_time = 200 * CLK_50MHZ_TICK_TO_MS_FACTOR;
    c->last_daq_sent = 0;
    c->last_credit_rcv = 0;
    c->hist_crcv[0] = 0;
    c->hist_crcv[1] = 0;
    c->hist_crcv[2] = 0;
    c->hist_crcv[3] = 0;
    c->cred_rtt = 0;

    c->exp_req_ix = 0;
    c->nxt_rep_ix = 0;
    c->nxt_rep_isfirst = 1;

    c->daq_miss_cnt = 0;
    c->daq_buf_pack = 0;
    c->daq_bufsz_pack = 0;
    c->resend_last = 0;
    c->state = READY_ACCEPT_CREDIT;
}

/*******************************************************************************
 Cmdi_Cmd_Interpret
*******************************************************************************/
int Cmdi_Cmd_Interpret(CmdiContext* c) {
    unsigned int param[10];
    int err = 0;
    int err2 = 0;
    char* rep;
    unsigned int val_rdi;
    char str[120];
    short* sh;
    int rep_ascii_len;
    char req_unit;
    unsigned char sid[8];
    unsigned short val_rds;
    float f;
    unsigned char uc;
    unsigned char req_seq;
    char* p;

    c->do_reply = 0;
    c->rep_size = 0;

    // Get a buffer for response if we do not have already one
    if (c->frrep == (void*) 0) {
        // Ask a buffer to the buffer manager
        if ((err = BufPool_GiveBuffer(c->bp, &(c->frrep), AUTO_RETURNED)) < 0) {
            c->err_cmd_cnt++;
            return (err);
        }
        // Leave space in the buffer for Ethernet IP and UDP headers
        p = (char*) c->frrep + USER_OFFSET_ETH_IP_UDP_HDR;
        // Zero the next two bytes which are reserved for future protocol
        *p = 0x00;
        p++;
        *p = 0x00;
        p++;
        // Set the effective address of the beginning of buffer for the user
        c->burep = (void*) p;
    }
    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME
    c->reply_is_cframe = 1;

    // Command to request some data
    if (strncmp(c->cmd, "daq", 3) == 0) {
        // Toggle state of time probe
        *((volatile unsigned int*) (XPAR_AXI_BRAM_CTRL_4_S_AXI_BASEADDR + 0x04)) = *((volatile unsigned int*) (XPAR_AXI_BRAM_CTRL_4_S_AXI_BASEADDR + 0x04)) ^ R1_TIME_PROBE;

        // Keep track of where responses will be sent
        c->daq_socket = c->lst_socket;

        // if ((err = sscanf((c->cmd+4), "%i %c %i", &param[0], &req_unit, &val_rdi)) >= 2)
        //  Get arguments in a faster way than using sscanf
        //  This requires that the format of the line is precisely:
        //  "daq 0xhhhhhh C 0xhh" or
        //  "daq 0xhhhhhh C"
        {
            err = 0;
            req_seq = 0;
            param[0] = 0;
            req_unit = '?';
            p = c->cmd + 3;
            if ((*(p) == ' ') && (*(p + 1) == '0') && (*(p + 2) == 'x')) {
                p += 3;
                ASCII2HEX(*(p), uc);
                param[0] = param[0] | (uc << 20);
                p++;
                ASCII2HEX(*(p), uc);
                param[0] = param[0] | (uc << 16);
                p++;
                ASCII2HEX(*(p), uc);
                param[0] = param[0] | (uc << 12);
                p++;
                ASCII2HEX(*(p), uc);
                param[0] = param[0] | (uc << 8);
                p++;
                ASCII2HEX(*(p), uc);
                param[0] = param[0] | (uc << 4);
                p++;
                ASCII2HEX(*(p), uc);
                param[0] = param[0] | (uc);
                p++;
                err++;
            }
            if (*(p) == ' ') {
                p++;
                req_unit = *p;
                p++;
                err++;
            }
            if ((*(p) == ' ') && (*(p + 1) == '0') && (*(p + 2) == 'x')) {
                p += 3;
                ASCII2HEX(*(p), uc);
                req_seq = req_seq | (uc << 4);
                p++;
                ASCII2HEX(*(p), uc);
                req_seq = req_seq | (uc);
                p++;
                err++;
            }
        }
        // printf("daq size: 0x%x unit:%c req_seq:0x%x\n", param[0], req_unit, req_seq);

        if (err >= 2) {
            // The request does not have any sequence number added
            if (err == 2) {
                c->exp_req_ix = 0;      // The next request will be 0
                c->nxt_rep_ix = 0;      // The first response will be 0
                c->nxt_rep_isfirst = 1; // The response will be the first one in this row
            }
            // A sequence number is added
            else {
                // Normally the sequence number is the expected one
                if (req_seq == c->exp_req_ix) {
                    // The request was delayed but not lost
                    // There can be two reasons that cannot be distinguished
                    // . the previous response was lost
                    // . nothing was lost but the delay was higher than the local timeout
                    if (c->state == CRED_RETURN_TIMED_OUT) {
                        c->rx_daq_delayed++;
                    }
                } else
                // The jump in index indicates how many daq requests were lost
                {
                    // The new index is greater than the current one: the difference
                    // is most likely the number of requests that were lost
                    if (req_seq > c->exp_req_ix) {
                        c->daq_miss_cnt += (req_seq - c->exp_req_ix);
                    }
                    // There was a wrap around in the index
                    else {
                        c->daq_miss_cnt += (256 + req_seq - c->exp_req_ix);
                    }
                    // printf("DAQ request missing count: %d\n", c->daq_miss_cnt);
                }
                // We re-synch the expected index to the last request index + 1
                c->exp_req_ix = req_seq + 1;
            }

            // The unit used for the request must be Bytes or Frames
            if (req_unit == 'B') {
                err = 0;
                c->request_unit = 'B';
                c->snd_allowed += param[0];
                c->do_reply = 0;
            } else if (req_unit == 'F') {
                err = 0;
                c->request_unit = 'F';
                c->snd_allowed += param[0];
                c->do_reply = 0;
            } else {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d): illegal request unit %c\n", err, c->fem->card_id, req_unit);
                c->rx_cmd_cnt++; // count this has a normal command
                c->do_reply = 1;
            }

            if (err == 0) {
                if (param[0] == 0xFFFFFF) {
                    c->snd_allowed = 0;
                    c->last_daq_sent = 0;
                    err = 0;
                    sprintf(rep, "%d Fem(%02d): daq paused\n", err, c->fem->card_id);
                    c->rx_cmd_cnt++; // count this has a normal command
                    c->do_reply = 1;
                } else {
                    c->rx_daq_cnt++;

                    // Note the time when credit was sent back
                    if (c->last_daq_sent) {
                        c->last_credit_rcv = finetime_get_tick(c->fem->reg[7]);
                        c->cred_rtt = finetime_diff_tick(c->last_daq_sent, c->last_credit_rcv);
                        c->last_daq_sent = 0;
                    }

                    c->state = READY_ACCEPT_CREDIT;

                    // It appears to be faster to return here rather than at the end
                    // of this function although no additional code is executed
                    return (0);
                }
            }
        } else if (err == 1) {
            err = ERR_ILLEGAL_PARAMETER;
            sprintf(rep, "%d Fem(%02d): missing unit in daq request\n", err, c->fem->card_id);
            c->rx_cmd_cnt++; // count this has a normal command
            c->do_reply = 1;
        } else {
            err = 0;
            sprintf(rep, "0 Fem(%02d) send_credits: %d %c\n", c->fem->card_id, c->snd_allowed, c->request_unit);
            c->rx_cmd_cnt++; // count this has a normal command
            c->do_reply = 1;
        }
    }

    // Command to print version, compilation date and time
    else if (strncmp(c->cmd, "version", 7) == 0) {
        c->rx_cmd_cnt++;
        c->do_reply = 1;
        sprintf(rep, "0 Fem(%02d): Software V%d.%d Compiled %s at %s\n",
                c->fem->card_id,
                SERVER_VERSION_MAJOR,
                SERVER_VERSION_MINOR,
                server_date,
                server_time);
    }

    // Command to set/show if all channels are forced ON (AGET mode only; always true for AFTER)
    else if (strncmp(c->cmd, "forceon_all", 11) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "forceon_all %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R0_FORCEON_ALL;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_FORCEON_ALL, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) ForceOn_All: %d\n",
                        c->fem->card_id,
                        0,
                        val_rdi,
                        val_rdi,
                        R0_GET_FORCEON_ALL(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to show/set a channel pedestal value in the active Feminos
    else if (strncmp(c->cmd, "ped", 3) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_ActOnThrPedHrule(c);
    }

    // Command to show/set a channel threshold value in the active Feminos
    else if (strncmp(c->cmd, "thr", 3) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_ActOnThrPedHrule(c);
    }

    // Command to show/set a channel forceon value in the active Feminos
    else if (strncmp(c->cmd, "forceon", 7) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_ActOnThrPedHrule(c);
    }

    // Command to show/set a channel forceoff value in the active Feminos
    else if (strncmp(c->cmd, "forceoff", 8) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_ActOnThrPedHrule(c);
    }

    // Command to read/write a configuration register in the Feminos
    else if (strncmp(c->cmd, "reg", 3) == 0) {
        c->rx_cmd_cnt++;
        // Try to scan 2 args, the second in hex
        if ((err = sscanf(c->cmd, "reg %i 0x%x", &param[0], &param[1])) == 2) {
            // OK we got 2 arguments
        } else {
            // Scan again 2 args with the second in unsigned dec
            err = sscanf(c->cmd, "reg %i %u", &param[0], &param[1]);
        }
        if (err == 2) {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, param[0], 0xFFFFFFFF, &param[1])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, param[0], param[1]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, param[0], 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d)\n", c->fem->card_id, param[0], val_rdi, val_rdi);
            }
        }
        c->do_reply = 1;
    }

    // Command to read/write test pattern RAM
    else if (strncmp(c->cmd, "tdata", 5) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_ActOnTestData(c);
    }

    // Command family "sca"
    else if (strncmp(c->cmd, "sca", 3) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_ScaCommands(c);
    }

    // Command to print help message
    else if (strncmp(c->cmd, "help", 4) == 0) {
        c->rx_cmd_cnt++;
        Cmdi_Cmd_Usage(rep, 256);
        c->do_reply = 1;
    }

    // Command to set/show mode of operation (AFTER or AGET)
    else if (strncmp(c->cmd, "mode", 4) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "mode %s", &str[0]) == 1) {
            if (strncmp(&str[0], "after", 5) == 0) {
                param[0] = R0_MODE_AFTER;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_MODE_AFTER, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
                c->mode = R0_GET_MODE(param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                if (R0_GET_MODE(val_rdi) == 1) {
                    sprintf(&str[0], "AFTER");
                } else {
                    sprintf(&str[0], "AGET");
                }
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Mode: %s\n",
                        c->fem->card_id,
                        0,
                        val_rdi,
                        val_rdi,
                        &str[0]);
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show if the per chip channel hit count are sent in the data or not
    else if (strncmp(c->cmd, "emit_hit_cnt", 12) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "emit_hit_cnt %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R0_EMIT_CH_HIT_CNT;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_EMIT_CH_HIT_CNT, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Emit_Hit_Cnt: %d\n",
                        c->fem->card_id,
                        0,
                        val_rdi,
                        val_rdi,
                        R0_GET_EMIT_CH_HIT_CNT(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show AGET reset channel sequence (2 or 4 cycles)
    else if (strncmp(c->cmd, "rst_len", 7) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "rst_len %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R0_RST_CHAN_CNT;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_RST_CHAN_CNT, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                if (R0_GET_RST_CHAN_CNT(val_rdi) == 1) {
                    sprintf(&str[0], "4");
                } else {
                    sprintf(&str[0], "2");
                }
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Aget_Rst_Cycles: %s\n",
                        c->fem->card_id,
                        0,
                        val_rdi,
                        val_rdi,
                        &str[0]);
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show is rst cycles are sent in the data or not
    else if (strncmp(c->cmd, "keep_rst", 8) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "keep_rst %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R0_KEEP_RST_CHAN;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_KEEP_RST_CHAN, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Keep_Rst_Chan: %d\n",
                        c->fem->card_id,
                        0,
                        val_rdi,
                        val_rdi,
                        R0_GET_KEEP_RST_CHAN(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show how many rst cycles are skipped
    else if (strncmp(c->cmd, "skip_rst", 8) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "skip_rst %i", &param[0]) == 1) {
            param[0] = R0_PUT_SKIP_RST_UNTIL(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_SKIP_RST_UNTIL_M, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Skip_Rst_Chan_Until: %d\n",
                        c->fem->card_id,
                        0,
                        val_rdi,
                        val_rdi,
                        R0_GET_SKIP_RST_UNTIL(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show global zero suppression
    else if (strncmp(c->cmd, "zero_suppress", 13) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "zero_suppress %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R0_ZERO_SUPPRESS;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_ZERO_SUPPRESS, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Zero_Suppress: %d\n",
                        c->fem->card_id,
                        0,
                        val_rdi,
                        val_rdi,
                        R0_GET_ZERO_SUPPRESS(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show global pedestal subtraction
    else if (strncmp(c->cmd, "subtract_ped", 12) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "subtract_ped %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R0_PED_SUBTRACT;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_PED_SUBTRACT, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Subtract_Ped: %d\n",
                        c->fem->card_id,
                        0,
                        val_rdi,
                        val_rdi,
                        R0_GET_PED_SUBTRACT(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show trigger generator rate
    else if (strncmp(c->cmd, "trig_rate", 9) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "trig_rate %i %i", &param[0], &param[1]) == 2) {
            param[0] = ((param[0] & 0x3) << 7) | ((param[1] & 0x7F));
            param[0] = R2_PUT_TRIG_RATE(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 2, R2_TRIG_RATE_M, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 2, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 2, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                param[0] = ((R2_GET_TRIG_RATE(val_rdi) >> 7) & 0x3);
                param[1] = ((R2_GET_TRIG_RATE(val_rdi) >> 0) & 0x7F);

                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Range: %d  Rate: %d (%.2f Hz)\n",
                        c->fem->card_id,
                        2,
                        val_rdi,
                        val_rdi,
                        param[0],
                        param[1],
                        trig_range_rate2freq(param[0], param[1]));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show trigger mask
    else if (strncmp(c->cmd, "trig_enable", 9) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "trig_enable %i", &param[0]) == 1) {
            param[0] = R2_PUT_TRIG_ENABLE(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 2, R2_TRIG_ENABLE_M, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 2, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 2, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Trig_Enable: 0x%x\n",
                        c->fem->card_id,
                        2,
                        val_rdi,
                        val_rdi,
                        R2_GET_TRIG_ENABLE(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show trigger delay
    else if (strncmp(c->cmd, "trig_delay", 10) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "trig_delay %i", &param[0]) == 1) {
            param[0] = R2_PUT_TRIG_DELAY(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 2, R2_TRIG_DELAY_M, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 2, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 2, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Trig_Delay: 0x%x (%d)\n",
                        c->fem->card_id,
                        2,
                        val_rdi,
                        val_rdi,
                        R2_GET_TRIG_DELAY(val_rdi),
                        R2_GET_TRIG_DELAY(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show event limit
    else if (strncmp(c->cmd, "event_limit", 11) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "event_limit %i", &param[0]) == 1) {
            param[0] = R2_PUT_EVENT_LIMIT(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 2, R2_EVENT_LIMIT_M, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 2, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 2, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                param[0] = R2_GET_EVENT_LIMIT(val_rdi);
                if (param[0] == 0) {
                    sprintf(&str[0], "infinity");
                } else if (param[0] == 1) {
                    sprintf(&str[0], "1");
                } else if (param[0] == 2) {
                    sprintf(&str[0], "10");
                } else if (param[0] == 3) {
                    sprintf(&str[0], "100");
                } else if (param[0] == 4) {
                    sprintf(&str[0], "1000");
                } else if (param[0] == 5) {
                    sprintf(&str[0], "10000");
                } else if (param[0] == 6) {
                    sprintf(&str[0], "100000");
                } else if (param[0] == 7) {
                    sprintf(&str[0], "1000000");
                } else {
                    sprintf(&str[0], "unknown");
                }
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Event_Limit: %s\n",
                        c->fem->card_id,
                        2,
                        val_rdi,
                        val_rdi,
                        &str[0]);
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show GEN_USE_LIMIT
    else if (strncmp(c->cmd, "gen_use_limit", 13) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "gen_use_limit %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R5_GEN_USE_LIMIT;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 5, R5_GEN_USE_LIMIT, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 5, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 5, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Gen_use_limit: %d\n",
                        c->fem->card_id,
                        5,
                        val_rdi,
                        val_rdi,
                        R5_GET_GEN_USE_LIMIT(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to enable/disable Hit Register Modification
    else if (strncmp(c->cmd, "modify_hit_reg", 14) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "modify_hit_reg %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R0_MODIFY_HIT_REG;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_MODIFY_HIT_REG, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 0, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 0, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Modify_Hit_Register: %d\n",
                        c->fem->card_id,
                        0,
                        val_rdi,
                        val_rdi,
                        R0_GET_MODIFY_HIT_REG(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to enable/disable Test Enable
    else if (strncmp(c->cmd, "test_enable", 11) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "test_enable %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R5_TEST_ENABLE;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 5, R5_TEST_ENABLE, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 5, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 5, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Test_Enable: %d\n",
                        c->fem->card_id,
                        5,
                        val_rdi,
                        val_rdi,
                        R5_GET_TEST_ENABLE(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to select Test Mode
    else if (strncmp(c->cmd, "test_mode", 9) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "test_mode %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R5_TEST_MODE;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 5, R5_TEST_MODE, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 5, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 5, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Test_Mode: %d\n",
                        c->fem->card_id,
                        5,
                        val_rdi,
                        val_rdi,
                        R5_GET_TEST_MODE(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to act on Test ZBT
    else if (strncmp(c->cmd, "test_zbt", 8) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "test_zbt %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R5_TEST_ZBT;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 5, R5_TEST_ZBT, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 5, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 5, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Test_Zbt: %d\n",
                        c->fem->card_id,
                        5,
                        val_rdi,
                        val_rdi,
                        R5_GET_TEST_ZBT(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to clear/get Busy (i.e. dead-time) Histogram
    else if (strncmp(c->cmd, "hbusy", 5) == 0) {
        c->rx_cmd_cnt++;
        if (strncmp(c->cmd, "hbusy clr", 9) == 0) {
            HistoInt_Clear(&busy_histogram);
            sprintf(rep, "0 Fem(%02d) cleared busy histogram\n", c->fem->card_id);
            err = 0;
        } else if (strncmp(c->cmd, "hbusy get", 9) == 0) {
            HistoInt_ComputeStatistics(&busy_histogram);
            err = Busymeter_PacketizeHisto(&busy_histogram, (void*) c->burep, POOL_BUFFER_SIZE, &(c->rep_size), c->fem->card_id);
            c->reply_is_cframe = 0;
        } else {
            err = -1;
            sprintf(rep, "%d Fem(%02d) illegal command %s\n", err, c->fem->card_id, c->cmd);
        }
        c->do_reply = 1;
    }

    // Command to set number of samples before and after threshold passed in zero-suppressed mode
    else if (strncmp(c->cmd, "zs_pre_post", 5) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "zs_pre_post %i %i", &param[0], &param[1]) == 2) {
            if ((param[0] > 15) || (param[1] > 16)) {
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d): parameter %d %d out of range [0;15] [0;16]\n", err, c->fem->card_id, param[0], param[1]);
            } else {
                param[1] = param[0] + param[1];
                param[2] = R5_PUT_ZS_POST_PRE(0, param[1], param[0]);
                if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 5, R5_ZS_POST_PRE_M, &param[2])) < 0) {
                    sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
                } else {
                    sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 5, param[2]);
                }
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 5, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                param[0] = R5_GET_ZS_PRE(val_rdi);
                param[1] = R5_GET_ZS_POST(val_rdi) - R5_GET_ZS_PRE(val_rdi);
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) ZS_Pre: %d ZS_Post: %d\n",
                        c->fem->card_id,
                        5,
                        val_rdi,
                        val_rdi,
                        param[0],
                        param[1]);
            }
        }
        c->do_reply = 1;
    }

    // Command to read/write AFTER registers
    else if (strncmp(c->cmd, "after", 5) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_AfterCommands(c);
    }

    // Command to read/write AGET registers
    else if (strncmp(c->cmd, "aget", 4) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_AgetCommands(c);
    }

    // Command to control Ring Buffer Operation
    else if (strncmp(c->cmd, "rbf", 3) == 0) {
        c->rx_cmd_cnt++;

        // View Ring Buffer configuration
        if (strncmp(c->cmd, "rbf config", 10) == 0) {
            AxiRingBuffer_GetConfiguration(c->ar, &param[0]);
            sprintf(rep, "0 Fem(%02d) AXI Ring Buffer Base:0x%08x Buf_Capacity:%d bytes Run:%d RetPnd:%d Mode:%s Timeout:%s\n",
                    c->fem->card_id,
                    AXIRBF_GET_BASE(param[0]),
                    AXIRBF_GET_CAPACITY(param[0]),
                    AXIRBF_GET_RUN(param[0]),
                    AXIRBF_GET_RETPND(param[0]),
                    &AxiRingBuffer_Timed2str[AXIRBF_GET_TIMED(param[0])][0],
                    &AxiRingBuffer_Timeval2str[AXIRBF_GET_TIMEVAL(param[0])][0]);
            err = 0;
        }
        // Suspend Ring Buffer
        else if (strncmp(c->cmd, "rbf suspend", 11) == 0) {
            AxiRingBuffer_IoCtrl(c->ar, AXIRBF_RUN, AXIRBF_STOP);
            sprintf(rep, "0 Fem(%02d) AXI Ring Buffer Suspended\n", c->fem->card_id);
            err = 0;
        }
        // Resume Ring Buffer
        else if (strncmp(c->cmd, "rbf resume", 10) == 0) {
            AxiRingBuffer_IoCtrl(c->ar, AXIRBF_RUN, AXIRBF_RUN);
            sprintf(rep, "0 Fem(%02d) AXI Ring Buffer Restarted\n", c->fem->card_id);
            err = 0;
        }
        // Reset Ring Buffer
        else if (strncmp(c->cmd, "rbf reset", 9) == 0) {
            AxiRingBuffer_IoCtrl(c->ar, AXIRBF_IOCONTROL_MASK, AXIRBF_STOP);
            AxiRingBuffer_IoCtrl(c->ar, AXIRBF_IOCONTROL_MASK, AXIRBF_RESET);
            AxiRingBuffer_IoCtrl(c->ar, AXIRBF_IOCONTROL_MASK, AXIRBF_STOP);
            sprintf(rep, "0 Fem(%02d) AXI Ring Buffer Reset\n", c->fem->card_id);
            err = 0;
        }
        // Return pending buffer from Ring Buffer
        else if (strncmp(c->cmd, "rbf getpnd", 10) == 0) {
            AxiRingBuffer_IoCtrl(c->ar, AXIRBF_RETPND, AXIRBF_RETPND);
            AxiRingBuffer_IoCtrl(c->ar, AXIRBF_RUN, AXIRBF_STOP);
            AxiRingBuffer_IoCtrl(c->ar, AXIRBF_RETPND, 0);
            sprintf(rep, "0 Fem(%02d) AXI Ring Buffer Pending Frame Sent\n", c->fem->card_id);
            err = 0;
        }
        // Write/Read Ring Buffer time mode
        else if (strncmp(c->cmd, "rbf timed", 9) == 0) {
            if (sscanf(c->cmd, "rbf timed %i", &param[0]) == 1) {
                if (param[0] == 1) {
                    param[0] = AXIRBF_TIMED;
                } else {
                    param[0] = 0;
                }
                AxiRingBuffer_IoCtrl(c->ar, AXIRBF_TIMED, param[0]);
                sprintf(rep, "0 Fem(%02d) set AXI Ring Buffer Mode: %s\n",
                        c->fem->card_id,
                        &AxiRingBuffer_Timed2str[AXIRBF_GET_TIMED(param[0])][0]);
            } else {
                AxiRingBuffer_GetConfiguration(c->ar, &param[0]);
                sprintf(rep, "0 Fem(%02d) AXI Ring Buffer Mode: %s\n",
                        c->fem->card_id,
                        &AxiRingBuffer_Timed2str[AXIRBF_GET_TIMED(param[0])][0]);
            }
            err = 0;
        }
        // Write/Read timeout
        else if (strncmp(c->cmd, "rbf timeval", 11) == 0) {
            if (sscanf(c->cmd, "rbf timeval %i", &param[0]) == 1) {
                param[1] = AXIRBF_PUT_TIMEVAL(0, param[0]);
                AxiRingBuffer_IoCtrl(c->ar, AXIRBF_TIMEVAL_MASK, param[1]);
                sprintf(rep, "0 Fem(%02d) set AXI Ring Buffer timeout to %s\n",
                        c->fem->card_id,
                        &AxiRingBuffer_Timeval2str[param[0]][0]);
            } else {
                AxiRingBuffer_GetConfiguration(c->ar, &param[0]);
                param[0] = AXIRBF_GET_TIMEVAL(param[0]);
                sprintf(rep, "0 Fem(%02d) AXI Ring Buffer Timeout: %d (%s)\n",
                        c->fem->card_id,
                        param[0],
                        &AxiRingBuffer_Timeval2str[param[0]][0]);
            }
            err = 0;
        } else {
            err = -1;
            sprintf(rep, "%d Fem(%02d) illegal command %s\n", err, c->fem->card_id, c->cmd);
        }
        c->do_reply = 1;
    }

    // Command to act on KEEP_FCO
    else if (strncmp(c->cmd, "keep_fco", 8) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "keep_fco %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R1_KEEP_FCO;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 1, R1_KEEP_FCO, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 1, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 1, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Keep_FCO: %d\n",
                        c->fem->card_id,
                        1,
                        val_rdi,
                        val_rdi,
                        R1_GET_KEEP_FCO(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to act on FEC Power Enable/Disable
    else if (strncmp(c->cmd, "fec_enable", 10) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "fec_enable %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R1_FEC_ENABLE;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 1, R1_FEC_ENABLE, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 1, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 1, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) FEC_Enable: %d\n",
                        c->fem->card_id,
                        1,
                        val_rdi,
                        val_rdi,
                        R1_GET_FEC_ENABLE(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to act on FEC Power Control Inverter
    else if (strncmp(c->cmd, "power_inv", 9) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "power_inv %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R1_FEC_POW_INV;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 1, R1_FEC_POW_INV, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 1, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 1, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Power_FEC: %d\n",
                        c->fem->card_id,
                        1,
                        val_rdi,
                        val_rdi,
                        R1_GET_FEC_POW_INV(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show pulser injection delay
    else if (strncmp(c->cmd, "pul_delay", 9) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "pul_delay %i", &param[0]) == 1) {
            param[0] = R3_PUT_PUL_DELAY(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 3, R3_PUL_DELAY_M, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 3, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 3, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Pul_Delay: 0x%x (%d)\n",
                        c->fem->card_id,
                        3,
                        val_rdi,
                        val_rdi,
                        R3_GET_PUL_DELAY(val_rdi),
                        R3_GET_PUL_DELAY(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show pulser enable
    else if (strncmp(c->cmd, "pul_enable", 10) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "pul_enable %i", &param[0]) == 1) {
            param[0] = R3_PUT_PUL_ENABLE(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 3, R3_PUL_ENABLE, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 3, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 3, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Pul_Enable: %d\n",
                        c->fem->card_id,
                        3,
                        val_rdi,
                        val_rdi,
                        R3_GET_PUL_ENABLE(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to effectively load pulser amplitude
    else if (strncmp(c->cmd, "pul_load", 8) == 0) {
        c->rx_cmd_cnt++;

        // Set GEN_GO High
        param[0] = R1_PUL_LOAD;
        if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 1, R1_PUL_LOAD, &param[0])) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
        } else {
            // Set GEN_GO Low
            param[0] = 0;
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 1, R1_PUL_LOAD, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x GEN_GO pulsed\n", c->fem->card_id, 1, param[0]);
            }
        }
        c->do_reply = 1;
    }

    // Command to set pulser amplitude
    else if (strncmp(c->cmd, "pul_amp", 7) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "pul_amp %i", &param[0]) == 1) {
            if ((err = Pulser_SetAmplitude(c->fem, (unsigned short) param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Pulser_SetAmplitude failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Pulser_Amplitude <- 0x%x\n", c->fem->card_id, param[0]);
            }
        } else {
            err = ERR_SYNTAX;
            sprintf(rep, "%d Fem(%02d) usage: pul_amp <0xAmplitude>\n", err, c->fem->card_id);
        }
        c->do_reply = 1;
    }

    // Command to direct data : keep locally or sent to DAQ
    else if (strncmp(c->cmd, "serve_target", 12) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "serve_target %i", &param[0]) == 1) {
            if ((param[0] >= RBF_DATA_TARGET_NULL) && (param[0] <= RBF_DATA_TARGET_HIT_HISTO)) {
                c->serve_target = param[0];
                err = 0;
                sprintf(rep, "%d Fem(%02d) Serve_Target <- %d\n", err, c->fem->card_id, c->serve_target);
            } else {
                err = -1;
                sprintf(rep, "%d Fem(%02d) Serve_Target: illegal target %d\n", err, c->fem->card_id, param[0]);
            }
        } else {
            err = 0;
            sprintf(rep, "%d Fem(%02d) Serve_Target: %d (%s)\n", err, c->fem->card_id, c->serve_target, &Cmdi_RbfTarget2str[c->serve_target][0]);
        }
        c->do_reply = 1;
    }

    // Command to operate on pedestal histograms
    else if (strncmp(c->cmd, "hped", 4) == 0) {
        c->rx_cmd_cnt++;
        err = Pedestal_IntrepretCommand(c);
    }

    // Command to set/show ASIC mask
    else if (strncmp(c->cmd, "asic_mask", 9) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "asic_mask %i", &param[0]) == 1) {
            param[0] = R1_PUT_ASIC_MASK(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 1, R1_ASIC_MASK_M, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 1, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 1, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Asic_Mask: 0x%x\n",
                        c->fem->card_id,
                        1,
                        val_rdi,
                        val_rdi,
                        R1_GET_ASIC_MASK(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to clear time stamp in software
    else if (strncmp(c->cmd, "clr tstamp", 10) == 0) {
        c->rx_cmd_cnt++;
        // Set SFT_CLR_TSTAMP to 1
        param[0] = R0_CLR_TSTAMP;
        if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_CLR_TSTAMP, &param[0])) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
        }
        // Set SFT_CLR_TSTAMP to 0
        param[0] = 0;
        if ((err2 = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_CLR_TSTAMP, &param[0])) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
        }
        if ((err == 0) && (err2 == 0)) {
            sprintf(rep, "0 Fem(%02d) Time-stamp Counter cleared\n", c->fem->card_id);
        }
        if (err2 < 0) {
            err = err2;
        }
        c->do_reply = 1;
    }

    // Command to clear event count in software
    else if (strncmp(c->cmd, "clr evcnt", 9) == 0) {
        c->rx_cmd_cnt++;
        // Set SFT_CLR_EVCNT to 1
        param[0] = R0_CLR_EVCNT;
        if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_CLR_EVCNT, &param[0])) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
        }
        // Set SFT_CLR_EVCNT to 0
        param[0] = 0;
        if ((err2 = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 0, R0_CLR_EVCNT, &param[0])) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
        }
        if ((err == 0) && (err2 == 0)) {
            sprintf(rep, "0 Fem(%02d) Event Counter cleared\n", c->fem->card_id);
        }
        if (err2 < 0) {
            err = err2;
        }
        c->do_reply = 1;
    }

    // Command to see if time stamp was set
    else if (strncmp(c->cmd, "tstamp_isset", 12) == 0) {
        c->rx_cmd_cnt++;

        if (strncmp(c->cmd, "tstamp_isset clr", 16) == 0) {
            // Set TSTAMP_CLR_ISSET to 1
            param[0] = R6_TSTAMP_CLR_ISSET;
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 6, R6_TSTAMP_CLR_ISSET, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            }
            // Set TSTAMP_CLR_ISSET to 0
            param[0] = 0;
            if ((err2 = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 6, R6_TSTAMP_CLR_ISSET, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            }
            if ((err == 0) && (err2 == 0)) {
                sprintf(rep, "0 Fem(%02d) Time-stamp is_set bit cleared\n", c->fem->card_id);
            }
            if (err2 < 0) {
                err = err2;
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 6, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Tstamp_IsSet: %d\n",
                        c->fem->card_id,
                        1,
                        val_rdi,
                        val_rdi,
                        R6_GET_TSTAMP_ISSET(val_rdi));
            }
        }

        c->do_reply = 1;
    }

    // Command to load time stamp initial value
    else if (strncmp(c->cmd, "tstamp_init", 11) == 0) {
        c->rx_cmd_cnt++;
        err = 0;
        if (sscanf(c->cmd, "tstamp_init %i %i %i", &param[2], &param[1], &param[0]) == 3) {
            for (param[3] = 2; param[3] != 0xFFFFFFFF; param[3]--) {
                for (param[4] = 0x8000; param[4] != 0; param[4] >>= 1) {
                    // Prepare next bit of serial data
                    if (param[param[3]] & param[4]) {
                        param[5] = R6_TS_INIT_SDAT;
                    } else {
                        param[5] = 0;
                    }

                    // Set SCLK low and put data
                    err -= Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 6, (R6_TS_INIT_SDAT | R6_TS_INIT_SCLK), &param[5]);

                    // Set SCLK high
                    param[5] |= R6_TS_INIT_SCLK;
                    err -= Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 6, (R6_TS_INIT_SDAT | R6_TS_INIT_SCLK), &param[5]);
                }
            }

            // Set SCLK and SDAT low
            param[5] = 0;
            err -= Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 6, (R6_TS_INIT_SDAT | R6_TS_INIT_SCLK), &param[5]);

            // See if there were no error
            if (err == 0) {
                sprintf(rep, "%d Fem(%02d): tstamp_init <- 0x%04x%04x%04x\n", err, c->fem->card_id, param[2], param[1], param[0]);
            } else {
                param[6] = -err;
                err = -1;
                sprintf(rep, "%d Fem(%02d): tstamp_init failed with %d errors\n", err, c->fem->card_id, param[6]);
            }
        } else {
            err = ERR_SYNTAX;
            sprintf(rep, "%d Fem(%02d): usage: tstamp_init 0xVal_msb 0xVal 0xVal_lsb\n", err, c->fem->card_id);
        }
        c->do_reply = 1;
    }

    // Command to operate on threshold turn-on histogram
    else if (strncmp(c->cmd, "shisto", 6) == 0) {
        c->rx_cmd_cnt++;
        err = FeminosShisto_IntrepretCommand(c);
    }

    // Command to list pedestal equalization constants or programmed thresholds
    else if (strncmp(c->cmd, "list", 4) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_ListCommand(c);
    }

    // Command to set/show bit error rate tester for TCM-Feminos link
    else if (strncmp(c->cmd, "tcm_bert", 8) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "tcm_bert %i", &param[0]) == 1) {
            param[0] = R6_PUT_TCM_BERT(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 6, R6_TCM_BERT, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 6, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 6, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Tcm_Bert: %d\n",
                        c->fem->card_id,
                        6,
                        val_rdi,
                        val_rdi,
                        R6_GET_TCM_BERT(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // TCM commands
    else if (strncmp(c->cmd, "tcm", 3) == 0) {
        c->rx_cmd_cnt++;
        // Act on Register #6
        param[0] = 6;

        if (sscanf(c->cmd, "tcm ignore %i", &param[1]) == 1) {
            // Prepare ignore argument
            param[2] = R6_PUT_TCM_IGNORE(0, param[1]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, param[0], R6_TCM_IGNORE, &param[2])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, param[0], param[2]);
            }
        } else if (strncmp(c->cmd, "tcm clr", 7) == 0) {
            // Activate counter clear
            param[1] = R6_PUT_CLR_COUNT(0, 1);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, param[0], R6_CLR_COUNT, &param[1])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
            } else {
                // Clear counter clear
                param[1] = R6_PUT_CLR_COUNT(0, 0);
                if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, param[0], R6_CLR_COUNT, &param[1])) < 0) {
                    sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
                } else {
                    sprintf(rep, "0 Fem(%02d) Reg(%d) <- pulsed CNT_CLR\n", c->fem->card_id, param[0]);
                }
            }
        } else {
            // Read register #6
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, param[0], 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
            } else {
                if (strncmp(c->cmd, "tcm rx", 6) == 0) {
                    sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (RX_count=%d)\n", c->fem->card_id, param[0], val_rdi, R6_GET_RX_COUNT(val_rdi));
                } else if (strncmp(c->cmd, "tcm err", 7) == 0) {
                    sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (RX_error_count=%d)\n", c->fem->card_id, param[0], val_rdi, R6_GET_RX_PERROR(val_rdi));
                } else if (strncmp(c->cmd, "tcm tx", 6) == 0) {
                    sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (TX_count=%d)\n", c->fem->card_id, param[0], val_rdi, R6_GET_TX_COUNT(val_rdi));
                } else if (strncmp(c->cmd, "tcm ignore", 10) == 0) {
                    sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (TCM_Ignore=%d)\n", c->fem->card_id, param[0], val_rdi, R6_GET_TCM_IGNORE(val_rdi));
                } else {
                    err = ERR_UNKNOWN_COMMAND;
                    sprintf(rep, "%d Fem(%02d): unknown command %s\n", err, c->fem->card_id, c->cmd);
                }
            }
        }
        c->do_reply = 1;
    }

    // Command to read/set DC balanced encoding
    else if (strncmp(c->cmd, "dcbal_enc", 9) == 0) {
        c->rx_cmd_cnt++;
        // Act on Register #6
        param[0] = 6;

        if (sscanf(c->cmd, "dcbal_enc %x", &param[1]) == 1) {
            param[1] = R6_PUT_DCBAL_ENC(0, param[1]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, param[0], R6_DCBAL_ENC, &param[1])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, param[0], param[1]);
            }
        } else {
            // Read register #6
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, param[0], 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (DC_Bal_Encoding=%d)\n", c->fem->card_id, param[0], val_rdi, R6_GET_DCBAL_ENC(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to read/set DC balanced decoding
    else if (strncmp(c->cmd, "dcbal_dec", 9) == 0) {
        c->rx_cmd_cnt++;
        // Act on Register #6
        param[0] = 6;

        if (sscanf(c->cmd, "dcbal_dec %x", &param[1]) == 1) {
            param[1] = R6_PUT_DCBAL_DEC(0, param[1]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, param[0], R6_DCBAL_DEC, &param[1])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, param[0], param[1]);
            }
        } else {
            // Read register #6
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, param[0], 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister %d failed\n", err, c->fem->card_id, param[0]);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (DC_Bal_Decoding=%d)\n", c->fem->card_id, param[0], val_rdi, R6_GET_DCBAL_DEC(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to act on command statistics
    else if (strncmp(c->cmd, "cmd", 3) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_CmdCommands(c);
    }

    // Command to clear/get Channel Hit Count Histogram
    else if (strncmp(c->cmd, "hhit", 4) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "hhit clr %s", &str[0]) == 1) {
            if (str[0] == '*') {
                HistoInt_Clear(&(ch_hit_cnt_histo[0]));
                HistoInt_Clear(&(ch_hit_cnt_histo[1]));
                HistoInt_Clear(&(ch_hit_cnt_histo[2]));
                HistoInt_Clear(&(ch_hit_cnt_histo[3]));
                sprintf(rep, "0 Fem(%02d) cleared 4 channel hit count histograms\n", c->fem->card_id);
                err = 0;
            } else if (sscanf(&str[0], "%i", &param[0]) == 1) {
                if (param[0] < 4) {
                    HistoInt_Clear(&(ch_hit_cnt_histo[param[0]]));
                    sprintf(rep, "0 Fem(%02d) cleared channel hit count histogram %d \n", c->fem->card_id, param[0]);
                    err = 0;
                } else {
                    err = -1;
                    sprintf(rep, "%d Fem(%02d) illegal parameter %s\n", err, c->fem->card_id, &str[0]);
                }
            } else {
                err = -1;
                sprintf(rep, "%d Fem(%02d) illegal parameter %s\n", err, c->fem->card_id, &str[0]);
            }
        } else if (sscanf(c->cmd, "hhit get %i", &param[0]) == 1) {
            if (param[0] < 4) {
                HistoInt_ComputeStatistics(&(ch_hit_cnt_histo[param[0]]));
                err = HitHisto_PacketizeHisto(&(ch_hit_cnt_histo[param[0]]), (unsigned short) param[0], (void*) c->burep, POOL_BUFFER_SIZE, &(c->rep_size), c->fem->card_id);
                c->reply_is_cframe = 0;
            } else {
                err = -1;
                sprintf(rep, "%d Fem(%02d) illegal parameter %s\n", err, c->fem->card_id, &str[0]);
            }
        } else {
            err = -1;
            sprintf(rep, "%d Fem(%02d) illegal command %s\n", err, c->fem->card_id, c->cmd);
        }
        c->do_reply = 1;
    }

    // Command to set/show how to handle empty channels - send them to DAQ or not?
    else if (strncmp(c->cmd, "emit_empty_ch", 13) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "emit_empty_ch %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R5_EMIT_EMPTY_CH;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 5, R5_EMIT_EMPTY_CH, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 5, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 5, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Emit_Empty_Channels: %d\n",
                        c->fem->card_id,
                        5,
                        val_rdi,
                        val_rdi,
                        R5_GET_EMIT_EMPTY_CH(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show how the option to force end of frame on end of event
    else if (strncmp(c->cmd, "eof_on_eoe", 10) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "eof_on_eoe %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R5_EOF_ON_EOE;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 5, R5_EOF_ON_EOE, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 5, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 5, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) EndOfFrame_On_EndOfEvent: %d\n",
                        c->fem->card_id,
                        5,
                        val_rdi,
                        val_rdi,
                        R5_GET_EOF_ON_EOE(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show the polarity of the zero-suppressor
    else if (strncmp(c->cmd, "polarity", 8) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_ActOnSettings(c);
    }

    // Command to set/show the threshold to erase hit register
    else if (strncmp(c->cmd, "erase_hit_thr", 13) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_ActOnSettings(c);
    }

    // Command to show Feminos state
    else if (strncmp(c->cmd, "state", 5) == 0) {
        c->rx_cmd_cnt++;
        if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 5, 0xFFFFFFFF, &val_rdi)) < 0) {
            sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
        } else {
            fstate2s(&str[0], val_rdi);
            sprintf(rep, "0 Fem(%02d) State = 0x%x (%s)\n",
                    c->fem->card_id,
                    R5_GET_STATE(val_rdi),
                    &str[0]);
        }
        c->do_reply = 1;
    }

    // Minibios Commands
    else if (strncmp(c->cmd, "minibios", 8) == 0) {
        c->rx_cmd_cnt++;
        err = Minibios_Command(&minibiosdata, ((c->cmd) + 9), str);
        sprintf(rep, "%d Fem(%02d): %s", err, c->fem->card_id, str);
        c->do_reply = 1;
    }

    // Command to set/show EMIT_LST_CELL_RD
    else if (strncmp(c->cmd, "emit_lst_cell_rd", 16) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "emit_lst_cell_rd %i", &param[0]) == 1) {
            if (param[0] == 1) {
                param[0] = R5_EMIT_LST_CELL_RD;
            } else {
                param[0] = 0;
            }
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 5, R5_EMIT_LST_CELL_RD, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 5, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 5, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Emit_Last_Cell_Read: %d\n",
                        c->fem->card_id,
                        5,
                        val_rdi,
                        val_rdi,
                        R5_GET_EMIT_LST_CELL_RD(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show loss policy
    else if (strncmp(c->cmd, "loss_policy", 11) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "loss_policy %i", &param[0]) == 1) {
            if (param[0] < 3) {
                err = 0;
                c->loss_policy = param[0];
                sprintf(rep, "%d Fem(%02d): Loss_Policy <- %d\n", err, c->fem->card_id, param[0]);
            } else {
                err = -1;
                sprintf(rep, "%d Fem(%02d) illegal parameter in %s\n", err, c->fem->card_id, c->cmd);
            }
        } else {
            err = 0;
            sprintf(rep, "%d Fem(%02d): Loss_Policy : %d (%s)\n",
                    err,
                    c->fem->card_id,
                    c->loss_policy,
                    &Cmdi_LossPolicy2str[c->loss_policy][0]);
        }
        c->do_reply = 1;
    }

    // Command to set/show cred_wait_time
    else if (strncmp(c->cmd, "cred_wait_time", 14) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "cred_wait_time %i", &param[0]) == 1) {
            err = 0;
            c->cred_wait_time = param[0] * CLK_50MHZ_TICK_TO_MS_FACTOR;
            sprintf(rep, "%d Fem(%02d): Credit_Wait_Time <- %d\n", err, c->fem->card_id, param[0]);
        } else {
            err = 0;
            sprintf(rep, "%d Fem(%02d): Credit_Wait_Time : %d ms\n",
                    err,
                    c->fem->card_id,
                    (c->cred_wait_time / CLK_50MHZ_TICK_TO_MS_FACTOR));
        }
        c->do_reply = 1;
    }

    // Command to read Serial Number of FEC (DS2438)
    else if (strncmp(c->cmd, "fec ID", 6) == 0) {
        if ((err = ow_GetSerialNum(c, 0, &sid[0])) < 0) {
            sprintf(rep, "%d Fem(%02d): ow_GetSerialNum failed %d\n", err, c->fem->card_id, err);
        } else {
            sprintf(rep, "0 Fem(%02d) FEC_ID: 0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                    c->fem->card_id,
                    sid[7],
                    sid[6],
                    sid[5],
                    sid[4],
                    sid[3],
                    sid[2],
                    sid[1],
                    sid[0]);
        }
        c->do_reply = 1;
    }

    // Command to read temperature/voltage/current of the FEC (DS2438)
    else if (strncmp(c->cmd, "fec", 3) == 0) {
        // Get the operation to perform
        if (c->cmd[4] == 'T') {
            param[0] = OW_CONV_TEMP;
        } else if (c->cmd[4] == 'V') {
            param[0] = OW_CONV_VDD;
        } else if (c->cmd[4] == 'A') {
            param[0] = OW_CONV_VAD;
        } else if (c->cmd[4] == 'I') {
            param[0] = OW_CONV_I;
        } else {
            err = -1;
            sprintf(rep, "%d Fem(%02d) syntax: fec <T|V|A|I|ID>\n", err, c->fem->card_id);
        }

        // Execute the operation
        if (err == 0) {
            if ((err = ow_GetConversion(c, param[0], 0, (short*) &val_rds)) < 0) {
                sprintf(rep, "%d Fem(%02d): ow_GetConversion failed\n", err, c->fem->card_id);
            } else {
                if (param[0] == OW_CONV_TEMP) {
                    f = ((float) val_rds) * 0.03125;
                    sprintf(rep, "0 Fem(%02d) FEC_T: %.3f degC\n", c->fem->card_id, f);
                } else if (param[0] == OW_CONV_VDD) {
                    f = ((float) val_rds) * 0.01;
                    sprintf(rep, "0 Fem(%02d) FEC_Vdd: %.3f V\n", c->fem->card_id, f);
                } else if (param[0] == OW_CONV_VAD) {
                    f = ((float) val_rds) * 0.01;
                    sprintf(rep, "0 Fem(%02d) FEC_Vad: %.3f V\n", c->fem->card_id, f);
                } else if (param[0] == OW_CONV_I) {
                    f = ((float) val_rds) / 204.8;
                    sprintf(rep, "0 Fem(%02d) FEC_I: %.3f A\n", c->fem->card_id, f);
                } else {
                    // should never get there
                    sprintf(rep, "0 Fem(%02d) should have never came there!\n", c->fem->card_id);
                }
            }
        }
        c->do_reply = 1;
    }

    // Command family "mars"
    else if (strncmp(c->cmd, "mars", 4) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_I2CCommands(c);
    }

    // Command family "flash"
    else if (strncmp(c->cmd, "flash", 5) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_FlashCommands(c);
    }

    // Command family "etherstat"
    else if (strncmp(c->cmd, "etherstat", 9) == 0) {
        c->rx_cmd_cnt++;
        if (strncmp(c->cmd + 9, " clr", 3) == 0) {
            Ethernet_StatClear(c->et);
            sprintf(rep, "0 Fem(%02d) Ethernet statistics cleared.\n", c->fem->card_id);

        } else {
            Ethernet_StatShow(c->et);
            sprintf(rep, "0 Fem(%02d) Ethernet statistics printed on console.\n", c->fem->card_id);
        }
        err = 0;
        c->do_reply = 1;
    }

    // Command to set/show multiplicity thresholds
    else if (strncmp(c->cmd, "mult_thr", 8) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_ActOnSettings(c);
    }

    // Command to set/show multiplicity limit
    else if (strncmp(c->cmd, "mult_limit", 10) == 0) {
        c->rx_cmd_cnt++;
        err = Cmdi_ActOnSettings(c);
    }

    // Command to set/show multiplicity self trigger enable
    else if (strncmp(c->cmd, "mult_trig_ena", 13) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "mult_trig_ena %i", &param[0]) == 1) {
            param[0] = R3_PUT_MULT_TRIG_ENA(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 3, R3_MULT_TRIG_ENA, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 3, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 3, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Mult_Trig_Ena: %d\n",
                        c->fem->card_id,
                        3,
                        val_rdi,
                        val_rdi,
                        R3_GET_MULT_TRIG_ENA(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show if multiplicity-over-threshold bits are sent to the TCM or not
    else if (strncmp(c->cmd, "snd_mult_ena", 12) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "snd_mult_ena %i", &param[0]) == 1) {
            param[0] = R3_PUT_SND_MULT_ENA(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 3, R3_SND_MULT_ENA, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 3, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 3, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Snd_Mult_Ena: %d\n",
                        c->fem->card_id,
                        3,
                        val_rdi,
                        val_rdi,
                        R3_GET_SND_MULT_ENA(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Command to set/show ERASE_HIT_ENA
    else if (strncmp(c->cmd, "erase_hit_ena", 13) == 0) {
        c->rx_cmd_cnt++;
        if (sscanf(c->cmd, "erase_hit_ena %i", &param[0]) == 1) {
            param[0] = R3_PUT_ERASE_HIT_ENA(0, param[0]);
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_WRITE, 3, R3_ERASE_HIT_ENA, &param[0])) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) <- 0x%x\n", c->fem->card_id, 3, param[0]);
            }
        } else {
            if ((err = Feminos_ActOnRegister(c->fem, FM_REG_READ, 3, 0xFFFFFFFF, &val_rdi)) < 0) {
                sprintf(rep, "%d Fem(%02d): Feminos_ActOnRegister failed\n", err, c->fem->card_id);
            } else {
                sprintf(rep, "0 Fem(%02d) Reg(%d) = 0x%x (%d) Erase_Hit_Ena: %d\n",
                        c->fem->card_id,
                        3,
                        val_rdi,
                        val_rdi,
                        R3_GET_ERASE_HIT_ENA(val_rdi));
            }
        }
        c->do_reply = 1;
    }

    // Unknown command
    else {
        err = ERR_UNKNOWN_COMMAND;
        c->rx_cmd_cnt++;
        c->do_reply = 1;
        sprintf(rep, "%d Fem(%02d) Unknown command: %s", ERR_UNKNOWN_COMMAND, c->fem->card_id, c->cmd);
    }

    // Format frame to be a configuration reply frame
    if (c->reply_is_cframe) {
        sh = (short*) c->burep;
        c->rep_size = 0;

        // Put CFrame prefix
        *sh = PUT_FVERSION_FEMID(PFX_START_OF_CFRAME, CURRENT_FRAMING_VERSION, c->fem->card_id);
        sh++;
        c->rep_size += 2;

        // Put error code
        *sh = err;
        sh++;
        c->rep_size += 2;

        // Get string size (excluding string null terminating character)
        rep_ascii_len = strlen((char*) (c->burep + CFRAME_ASCII_MSG_OFFSET));
        // Include the size of the null character in Frame size
        c->rep_size = c->rep_size + rep_ascii_len + 1;

        // Put string size
        *sh = PUT_ASCII_LEN(rep_ascii_len);
        sh++;
        c->rep_size += 2;

        // If response size is odd, add one more null word and make even response size
        if (c->rep_size & 0x1) {
            *((char*) (c->burep + CFRAME_ASCII_MSG_OFFSET + rep_ascii_len)) = '\0';
            c->rep_size++;
        }

        // Put end of frame
        sh = (short*) (c->burep + c->rep_size);
        *sh = PFX_END_OF_FRAME;
        c->rep_size += 2;

        // Include in the size the 2 empty bytes kept at the beginning of the UDP datagram
        c->rep_size += 2;

        // The response (if any) is sent to whom sent the last command
    }

    // Increment command error count if needed
    if (err < 0) {
        c->err_cmd_cnt++;
    }

    return (err);
}
