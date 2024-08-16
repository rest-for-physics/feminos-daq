/*******************************************************************************

                           _____________________

 File:        cmdi_after.c

 Description: Command interpreter for Feminos card.
  Implementation of commands specific to the AFTER chip

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr

 History:
  January 2012: created with code moved from cmdi.c

  July 2013: added "test_mode" command

*******************************************************************************/

#include "cmdi_after.h"
#include "after.h"
#include "after_strings.h"
#include "cmdi.h"
#include "error_codes.h"

#include <stdio.h>
#include <string.h>

/*******************************************************************************
 Cmdi_AfterCommands
*******************************************************************************/
int Cmdi_AfterCommands(CmdiContext* c) {
    unsigned int param[10];
    int err = 0;
    char strr[8][16];
    char* rep;
    unsigned short val_rd[8];
    unsigned short val_wr[8];
    int action, action_type;
    int arg_cnt;
    unsigned short asic_cur, asic_beg, asic_end;
    unsigned short reg_adr;
    int chip_wr, chip_ck;
    unsigned short gain, stime, marker, test_mode;

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME

    // Determine what action is requested
    action = -1;
    action_type = -1;
    reg_adr = -1;
    gain = 0;
    stime = 0;
    marker = 0;
    test_mode = 0;
    arg_cnt = 0;
    if ((arg_cnt = sscanf(c->cmd, "after %s %s %s %s %s %s", &strr[0][0], &strr[1][0], &strr[2][0], &strr[3][0], &strr[4][0], &strr[5][0])) >= 2) {
        if (strncmp(&strr[1][0], "read", 4) == 0) {
            action = 1;
        } else if (strncmp(&strr[1][0], "write", 5) == 0) {
            action = 2;
        } else if (strncmp(&strr[1][0], "wrchk", 5) == 0) {
            action = 3;
        } else if (strncmp(&strr[1][0], "gain", 4) == 0) {
            action = 4;
        } else if (strncmp(&strr[1][0], "time", 4) == 0) {
            action = 5;
        } else if (strncmp(&strr[1][0], "en_mkr_rst", 10) == 0) {
            action = 6;
        } else if (strncmp(&strr[1][0], "rst_level", 9) == 0) {
            action = 7;
        } else if (strncmp(&strr[1][0], "rd_from_0", 9) == 0) {
            action = 8;
        } else if (strncmp(&strr[1][0], "test_digout", 11) == 0) {
            action = 9;
        } else if (strncmp(&strr[1][0], "test_mode", 9) == 0) {
            action = 10;
        } else {
            err = ERR_ILLEGAL_PARAMETER;
            sprintf(rep, "%d Fem(%02d) After(%s) : illegal action %s\n",
                    err,
                    c->fem->card_id,
                    &strr[0][0],
                    &strr[1][0]);
            c->do_reply = 1;
            return (err);
        }

        // Determine to which AFTER this action applies
        if (strncmp(&strr[0][0], "*", 1) == 0) {
            asic_beg = 0;
            asic_end = MAX_NB_OF_AFTER_PER_FEMINOS - 1;
        } else if (sscanf(&strr[0][0], "%i:%i", &param[0], &param[1]) == 2) {
            // swap if needed
            if (param[0] < param[1]) {
                asic_beg = (unsigned short) param[0];
                asic_end = (unsigned short) param[1];
            } else {
                asic_beg = (unsigned short) param[1];
                asic_end = (unsigned short) param[0];
            }
        } else if (sscanf(&strr[0][0], "%i", &param[0]) == 1) {
            asic_beg = (unsigned short) param[0];
            asic_end = (unsigned short) param[0];
        } else {
            err = ERR_ILLEGAL_PARAMETER;
            sprintf(rep, "%d Fem(%02d) After(?) : illegal parameter %s\n",
                    err,
                    c->fem->card_id,
                    &strr[0][0]);
            c->do_reply = 1;
            return (err);
        }
    } else {
        err = ERR_SYNTAX;
        sprintf(rep, "%d Fem(%02d) usage: after <id> <action> <arguments>\n", err, c->fem->card_id);
        c->do_reply = 1;
        return (err);
    }

    // Get the register address for read, write and wrchk actions
    if ((action == 1) || (action == 2) || (action == 3)) {
        if (arg_cnt <= 2) {
            err = ERR_SYNTAX;
            sprintf(rep, "%d Fem(%02d) usage: after <id> <action> <arguments>\n", err, c->fem->card_id);
            c->do_reply = 1;
            return (err);
        }
        // Get the register address
        if (sscanf(&strr[2][0], "%i", &param[0]) == 1) {
            reg_adr = (unsigned short) param[0];
        } else {
            err = ERR_SYNTAX;
            sprintf(rep, "%d Fem(%02d) After(%s) : syntax error in %s\n",
                    err,
                    c->fem->card_id,
                    &strr[0][0],
                    &strr[2][0]);
            c->do_reply = 1;
            return (err);
        }

        // Check the register address
        if (reg_adr > ASIC_REGISTER_MAX_ADDRESS) {
            err = ERR_ILLEGAL_PARAMETER;
            sprintf(rep, "%d Fem(%02d) After(%s) : %s illegal register %d\n",
                    err,
                    c->fem->card_id,
                    &strr[0][0],
                    &strr[1][0],
                    reg_adr);
            c->do_reply = 1;
            return (err);
        }
    }
    // Get the gain for the gain action
    else if (action == 4) {
        reg_adr = 1;
        if (arg_cnt <= 2) {
            action_type = 0; // read
        } else {
            action_type = 1; // write

            // Get the gain
            if (sscanf(&strr[2][0], "%i", &param[0]) == 1) {
                if (param[0] == 120) {
                    gain = 0;
                } else if (param[0] == 240) {
                    gain = 1;
                } else if (param[0] == 360) {
                    gain = 2;
                } else if (param[0] == 600) {
                    gain = 3;
                } else {
                    err = ERR_ILLEGAL_PARAMETER;
                    sprintf(rep, "%d Fem(%02d) After(%s) : %s illegal gain %d\n",
                            err,
                            c->fem->card_id,
                            &strr[0][0],
                            &strr[1][0],
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            } else {
                err = ERR_SYNTAX;
                sprintf(rep, "%d Fem(%02d) After(%s) : syntax error in %s\n",
                        err,
                        c->fem->card_id,
                        &strr[0][0],
                        &strr[2][0]);
                c->do_reply = 1;
                return (err);
            }
        }
    }
    // Get the shaping time for the time action
    else if (action == 5) {
        reg_adr = 1;
        if (arg_cnt <= 2) {
            action_type = 0; // read
        } else {
            action_type = 1; // write

            // Get the shaping time
            if (sscanf(&strr[2][0], "%i", &param[0]) == 1) {
                if (param[0] <= 116) {
                    stime = 0;
                } else if (param[0] <= 200) {
                    stime = 1;
                } else if (param[0] <= 412) {
                    stime = 2;
                } else if (param[0] <= 505) {
                    stime = 3;
                } else if (param[0] <= 610) {
                    stime = 4;
                } else if (param[0] <= 695) {
                    stime = 5;
                } else if (param[0] <= 912) {
                    stime = 6;
                } else if (param[0] <= 993) {
                    stime = 7;
                } else if (param[0] <= 1054) {
                    stime = 8;
                } else if (param[0] <= 1134) {
                    stime = 9;
                } else if (param[0] <= 1343) {
                    stime = 10;
                } else if (param[0] <= 1421) {
                    stime = 11;
                } else if (param[0] <= 1546) {
                    stime = 12;
                } else if (param[0] <= 1626) {
                    stime = 13;
                } else if (param[0] <= 1834) {
                    stime = 14;
                } else if (param[0] <= 1912) {
                    stime = 15;
                } else {
                    err = ERR_ILLEGAL_PARAMETER;
                    sprintf(rep, "%d Fem(%02d) After(%s) : %s illegal shaping time %d\n",
                            err,
                            c->fem->card_id,
                            &strr[0][0],
                            &strr[1][0],
                            param[0]);
                    c->do_reply = 1;
                    return (err);
                }
            } else {
                err = ERR_SYNTAX;
                sprintf(rep, "%d Fem(%02d) After(%s) : syntax error in %s\n",
                        err,
                        c->fem->card_id,
                        &strr[0][0],
                        &strr[2][0]);
                c->do_reply = 1;
                return (err);
            }
        }
    }
    // Get the marker bit for the marker action
    else if (action == 6) {
        reg_adr = 2;
        if (arg_cnt <= 2) {
            action_type = 0; // read
        } else {
            action_type = 1; // write

            // Get the marker enable/disable bit
            if (sscanf(&strr[2][0], "%i", &param[0]) == 1) {
                marker = param[0];
            } else {
                err = ERR_SYNTAX;
                sprintf(rep, "%d Fem(%02d) After(%s) : syntax error in %s\n",
                        err,
                        c->fem->card_id,
                        &strr[0][0],
                        &strr[2][0]);
                c->do_reply = 1;
                return (err);
            }
        }
    }
    // Get the marker bit level for the marker action
    else if (action == 7) {
        reg_adr = 2;
        if (arg_cnt <= 2) {
            action_type = 0; // read
        } else {
            action_type = 1; // write

            // Get the marker level bit
            if (sscanf(&strr[2][0], "%i", &param[0]) == 1) {
                marker = param[0];
            } else {
                err = ERR_SYNTAX;
                sprintf(rep, "%d Fem(%02d) After(%s) : syntax error in %s\n",
                        err,
                        c->fem->card_id,
                        &strr[0][0],
                        &strr[2][0]);
                c->do_reply = 1;
                return (err);
            }
        }
    }
    // Operate on the rd_from_0 bit
    else if (action == 8) {
        reg_adr = 2;
        if (arg_cnt <= 2) {
            action_type = 0; // read
        } else {
            action_type = 1; // write

            // Get the rd_from_0 bit
            if (sscanf(&strr[2][0], "%i", &param[0]) == 1) {
                marker = param[0];
            } else {
                err = ERR_SYNTAX;
                sprintf(rep, "%d Fem(%02d) After(%s) : syntax error in %s\n",
                        err,
                        c->fem->card_id,
                        &strr[0][0],
                        &strr[2][0]);
                c->do_reply = 1;
                return (err);
            }
        }
    }
    // Operate on the test_digout bit
    else if (action == 9) {
        reg_adr = 2;
        if (arg_cnt <= 2) {
            action_type = 0; // read
        } else {
            action_type = 1; // write

            // Get the test_digout bit
            if (sscanf(&strr[2][0], "%i", &param[0]) == 1) {
                marker = param[0];
            } else {
                err = ERR_SYNTAX;
                sprintf(rep, "%d Fem(%02d) After(%s) : syntax error in %s\n",
                        err,
                        c->fem->card_id,
                        &strr[0][0],
                        &strr[2][0]);
                c->do_reply = 1;
                return (err);
            }
        }
    }
    // Get the test mode for the test_mode action
    else if (action == 10) {
        reg_adr = 1;
        if (arg_cnt <= 2) {
            action_type = 0; // read
        } else {
            action_type = 1; // write

            // Get the test mode
            if (sscanf(&strr[2][0], "%i", &param[0]) == 1) {
                test_mode = param[0];
            } else {
                err = ERR_SYNTAX;
                sprintf(rep, "%d Fem(%02d) After(%s) : syntax error in %s\n",
                        err,
                        c->fem->card_id,
                        &strr[0][0],
                        &strr[2][0]);
                c->do_reply = 1;
                return (err);
            }
        }
    }

    // Get the arguments for write and wrchk actions
    if ((action == 2) || (action == 3)) {
        // Get parameters for 16-bit registers
        if ((reg_adr == 1) || (reg_adr == 2)) {
            // Check that we have enough arguments
            if (arg_cnt <= 3) {
                err = ERR_SYNTAX;
                sprintf(rep, "%d usage: after <id> <action> <arguments>\n", err);
                c->do_reply = 1;
                return (err);
            }
            // Get the 16-bit value to write
            if (sscanf(&strr[3][0], "%i", &param[0]) == 1) {
                val_wr[0] = (unsigned short) param[0];
            } else {
                err = ERR_SYNTAX;
                sprintf(rep, "%d Fem(%02d) After(%s) : %s %s syntax error in %s\n",
                        err,
                        c->fem->card_id,
                        &strr[0][0],
                        &strr[1][0],
                        &strr[2][0],
                        &strr[3][0]);
                c->do_reply = 1;
                return (err);
            }
        }
        // Get parameters for 38-bit registers
        else if ((reg_adr == 3) || (reg_adr == 4)) {
            // Check that we have enough arguments
            if (arg_cnt <= 5) {
                err = ERR_SYNTAX;
                sprintf(rep, "%d usage: after <id> <action> <arguments>\n", err);
                c->do_reply = 1;
                return (err);
            }
            // Get the 38-bit value to write
            err = sscanf(&strr[3][0], "%i", &param[0]);
            err += sscanf(&strr[4][0], "%i", &param[1]);
            err += sscanf(&strr[5][0], "%i", &param[2]);
            if (err == 3) {
                val_wr[0] = (unsigned short) param[0];
                val_wr[1] = (unsigned short) param[1];
                val_wr[2] = (unsigned short) param[2];
            } else {
                err = ERR_SYNTAX;
                sprintf(rep, "%d Fem(%02d) After(%s) : %s %s syntax error in %s %s %s\n",
                        err,
                        c->fem->card_id,
                        &strr[0][0],
                        &strr[1][0],
                        &strr[2][0],
                        &strr[3][0],
                        &strr[4][0],
                        &strr[5][0]);
                c->do_reply = 1;
                return (err);
            }
        }
    }

    // For read actions, only one ASIC can be worked on
    if ((action == 1) ||
        ((action == 4) && (action_type == 0)) ||
        ((action == 5) && (action_type == 0)) ||
        ((action == 6) && (action_type == 0)) ||
        ((action == 7) && (action_type == 0)) ||
        ((action == 8) && (action_type == 0)) ||
        ((action == 9) && (action_type == 0)) ||
        ((action == 10) && (action_type == 0))) {
        if (asic_beg != asic_end) {
            err = ERR_ILLEGAL_PARAMETER;
            sprintf(rep, "%d Fem(%02d) After(%s) : %s multiple target is illegal\n",
                    err,
                    c->fem->card_id,
                    &strr[0][0],
                    &strr[1][0]);
            c->do_reply = 1;
            return (err);
        }
    }

    // Loop on all ASICs
    chip_wr = 0;
    chip_ck = 0;
    for (asic_cur = asic_beg; asic_cur <= asic_end; asic_cur++) {
        // for write gain action
        if ((action == 4) && (action_type == 1)) {
            // Prepare the value to be written
            val_wr[0] = (unsigned short) AFTER_PUT_GAIN(c->fem->asics[asic_cur].bar[reg_adr].bars[0], gain);
        }
        // for write shaping time action
        else if ((action == 5) && (action_type == 1)) {
            // Prepare the value to be written
            val_wr[0] = (unsigned short) AFTER_PUT_STIME(c->fem->asics[asic_cur].bar[reg_adr].bars[0], stime);
        }
        // for write marker enable action
        else if ((action == 6) && (action_type == 1)) {
            // Prepare the value to be written
            val_wr[0] = (unsigned short) AFTER_PUT_MARKER(c->fem->asics[asic_cur].bar[reg_adr].bars[0], marker);
        }
        // for write marker level action
        else if ((action == 7) && (action_type == 1)) {
            // Prepare the value to be written
            val_wr[0] = (unsigned short) AFTER_PUT_MARK_LVL(c->fem->asics[asic_cur].bar[reg_adr].bars[0], marker);
        }
        // for rd_from_0 action
        else if ((action == 8) && (action_type == 1)) {
            // Prepare the value to be written
            val_wr[0] = (unsigned short) AFTER_PUT_RD_FROM_0(c->fem->asics[asic_cur].bar[reg_adr].bars[0], marker);
        }
        // for test_digout action
        else if ((action == 9) && (action_type == 1)) {
            // Prepare the value to be written
            val_wr[0] = (unsigned short) AFTER_PUT_TEST_DIGOUT(c->fem->asics[asic_cur].bar[reg_adr].bars[0], marker);
        }
        // for test_mode action
        else if ((action == 10) && (action_type == 1)) {
            // Prepare the value to be written
            val_wr[0] = (unsigned short) AFTER_PUT_TEST_MODE(c->fem->asics[asic_cur].bar[reg_adr].bars[0], test_mode);
        }
        // For write and wrchk actions: Write ASIC # register # (16-bit or 38-bit)
        if ((action == 2) ||
            (action == 3) ||
            ((action == 4) && (action_type == 1)) ||
            ((action == 5) && (action_type == 1)) ||
            ((action == 6) && (action_type == 1)) ||
            ((action == 7) && (action_type == 1)) ||
            ((action == 8) && (action_type == 1)) ||
            ((action == 9) && (action_type == 1)) ||
            ((action == 10) && (action_type == 1))) {
            if ((err = After_Write(c->fem, asic_cur, reg_adr, &val_wr[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) After(%d) : After_Write failed\n",
                        err,
                        c->fem->card_id,
                        asic_cur);
                c->do_reply = 1;
                return (err);
            } else {
                chip_wr++;
            }
        }

        // For read and wrchk: Read ASIC # register # (16-bit or 38-bit)
        if ((action == 1) || (action == 3)) {
            val_rd[0] = 0x0000;
            val_rd[1] = 0x0000;
            val_rd[2] = 0x0000;
            if ((err = After_Read(c->fem, asic_cur, reg_adr, &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) After(%d): After_Read failed\n",
                        err,
                        c->fem->card_id,
                        asic_cur);
                c->do_reply = 1;
                return (err);
            }
        }

        // For read gain: Read ASIC # register 1 (16-bit)
        if ((action == 4) && (action_type == 0)) {
            val_rd[0] = 0x0000;
            val_rd[1] = 0x0000;
            val_rd[2] = 0x0000;
            if ((err = After_Read(c->fem, asic_cur, reg_adr, &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) After(%d): After_Read failed\n",
                        err,
                        c->fem->card_id,
                        asic_cur);
                c->do_reply = 1;
                return (err);
            }
        }
        // For read shaping time: Read ASIC # register 1 (16-bit)
        else if ((action == 5) && (action_type == 0)) {
            val_rd[0] = 0x0000;
            val_rd[1] = 0x0000;
            val_rd[2] = 0x0000;
            if ((err = After_Read(c->fem, asic_cur, reg_adr, &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) After(%d): After_Read failed\n",
                        err,
                        c->fem->card_id,
                        asic_cur);
                c->do_reply = 1;
                return (err);
            }
        }
        // For read marker : Read ASIC # register 2 (16-bit)
        else if ((action == 6) && (action_type == 0)) {
            val_rd[0] = 0x0000;
            val_rd[1] = 0x0000;
            val_rd[2] = 0x0000;
            if ((err = After_Read(c->fem, asic_cur, reg_adr, &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) After(%d): After_Read failed\n",
                        err,
                        c->fem->card_id,
                        asic_cur);
                c->do_reply = 1;
                return (err);
            }
        }
        // For read marker level : Read ASIC # register 2 (16-bit)
        else if ((action == 7) && (action_type == 0)) {
            val_rd[0] = 0x0000;
            val_rd[1] = 0x0000;
            val_rd[2] = 0x0000;
            if ((err = After_Read(c->fem, asic_cur, reg_adr, &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) After(%d): After_Read failed\n",
                        err,
                        c->fem->card_id,
                        asic_cur);
                c->do_reply = 1;
                return (err);
            }
        }
        // For read rd_from_0 : Read ASIC # register 2 (16-bit)
        else if ((action == 8) && (action_type == 0)) {
            val_rd[0] = 0x0000;
            val_rd[1] = 0x0000;
            val_rd[2] = 0x0000;
            if ((err = After_Read(c->fem, asic_cur, reg_adr, &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) After(%d): After_Read failed\n",
                        err,
                        c->fem->card_id,
                        asic_cur);
                c->do_reply = 1;
                return (err);
            }
        }
        // For read test_digout : Read ASIC # register 2 (16-bit)
        else if ((action == 9) && (action_type == 0)) {
            val_rd[0] = 0x0000;
            val_rd[1] = 0x0000;
            val_rd[2] = 0x0000;
            if ((err = After_Read(c->fem, asic_cur, reg_adr, &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) After(%d): After_Read failed\n",
                        err,
                        c->fem->card_id,
                        asic_cur);
                c->do_reply = 1;
                return (err);
            }
        }
        // For read test_mode : Read ASIC # register 2 (16-bit)
        else if ((action == 10) && (action_type == 0)) {
            val_rd[0] = 0x0000;
            val_rd[1] = 0x0000;
            val_rd[2] = 0x0000;
            if ((err = After_Read(c->fem, asic_cur, reg_adr, &val_rd[0])) < 0) {
                sprintf(rep, "%d Fem(%02d) After(%d): After_Read failed\n",
                        err,
                        c->fem->card_id,
                        asic_cur);
                c->do_reply = 1;
                return (err);
            }
        }

        // For action verify: compare result read to write
        if (action == 3) {
            // Compare values for 16-bit registers
            if ((reg_adr == 1) || (reg_adr == 2)) {
                // Check mismatch
                if (val_rd[0] != val_wr[0]) {
                    err = ERR_VERIFY_MISMATCH;
                    sprintf(rep, "%d Fem(%02d) After(%d) Reg(%d) Verify mismatch: WR 0x%x RD 0x%x\n",
                            err,
                            c->fem->card_id,
                            asic_cur,
                            reg_adr,
                            val_wr[0],
                            val_rd[0]);
                    c->do_reply = 1;
                    return (err);
                } else {
                    chip_ck++;
                }
            }
            // Compare values for 38-bit registers
            else if ((reg_adr == 3) || (reg_adr == 4)) {
                if ((val_rd[0] != val_wr[0]) || (val_rd[1] != val_wr[1]) || (val_rd[2] != val_wr[2])) {
                    err = ERR_VERIFY_MISMATCH;
                    sprintf(rep, "%d Fem(%02d) After(%d) Reg(%d) Verify mismatch: WR 0x%x 0x%x 0x%x RD 0x%x 0x%x 0x%x\n",
                            err,
                            c->fem->card_id,
                            asic_cur,
                            reg_adr,
                            val_wr[0],
                            val_wr[1],
                            val_wr[2],
                            val_rd[0],
                            val_rd[1],
                            val_rd[2]);
                    c->do_reply = 1;
                    return (err);
                } else {
                    chip_ck++;
                }
            } else {
                // Should not come here!
                err = ERR_ILLEGAL_PARAMETER;
                sprintf(rep, "%d Fem(%02d) After(%s) : illegal case reached!\n",
                        err,
                        c->fem->card_id,
                        &strr[0][0]);
                c->do_reply = 1;
                return (err);
            }
        }
    }

    // For action read: publish result and return
    if (action == 1) {
        if ((reg_adr == 1) || (reg_adr == 2)) {
            sprintf(rep, "0 Fem(%02d) After(%d) Reg(%d): 0x%x\n",
                    c->fem->card_id,
                    asic_beg,
                    reg_adr,
                    val_rd[0]);
        } else {
            sprintf(rep, "0 Fem(%02d) After(%d) Reg(%d): 0x%x 0x%x 0x%x\n",
                    c->fem->card_id,
                    asic_beg,
                    reg_adr,
                    val_rd[0],
                    val_rd[1],
                    val_rd[2]);
        }
        c->do_reply = 1;
        return (0);
    }
    // For action write: publish results and return
    else if (action == 2) {
        if ((reg_adr == 1) || (reg_adr == 2)) {
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) <- 0x%x (wrote %d chip(s))\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    val_wr[0],
                    chip_wr);
        } else {
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) <- 0x%x 0x%x 0x%x (wrote %d chip(s))\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    val_wr[0],
                    val_wr[1],
                    val_wr[2],
                    chip_wr);
        }
        c->do_reply = 1;
        return (0);
    }
    // For action wrchk: publish results and return
    else if (action == 3) {
        if ((reg_adr == 1) || (reg_adr == 2)) {
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) <- 0x%x (%d chip verified)\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    val_wr[0],
                    chip_ck);
        } else {
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) <- 0x%x 0x%x 0x%x (%d chip verified)\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    val_wr[0],
                    val_wr[1],
                    val_wr[2],
                    chip_ck);
        }
        c->do_reply = 1;
        return (0);
    }
    // For action gain (read or write): publish results and return
    else if (action == 4) {
        if (action_type == 0) {
            gain = AFTER_GET_GAIN(val_rd[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) = 0x%x Gain=%s\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    val_rd[0],
                    &After_Gain2str[gain][0]);
        } else {
            gain = AFTER_GET_GAIN(val_wr[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) <- Gain=%s\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    &After_Gain2str[gain][0]);
        }
        c->do_reply = 1;
        return (0);
    }
    // For action shaping time (read or write): publish results and return
    else if (action == 5) {
        if (action_type == 0) {
            stime = AFTER_GET_STIME(val_rd[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) = 0x%x Shaping=%s\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    val_rd[0],
                    &After_Stime2str[stime][0]);
        } else {
            stime = AFTER_GET_STIME(val_wr[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) <- Shaping=%s\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    &After_Stime2str[stime][0]);
        }
        c->do_reply = 1;
        return (0);
    }
    // For action marker (read or write): publish results and return
    else if (action == 6) {
        if (action_type == 0) {
            marker = AFTER_GET_MARKER(val_rd[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) = 0x%x Marker=%d\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    val_rd[0],
                    marker);
        } else {
            marker = AFTER_GET_MARKER(val_wr[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) <- Marker=%d\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    marker);
        }
        c->do_reply = 1;
        return (0);
    }
    // For action marker level(read or write): publish results and return
    else if (action == 7) {
        if (action_type == 0) {
            marker = AFTER_GET_MARK_LVL(val_rd[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) = 0x%x Marker_Level=%d\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    val_rd[0],
                    marker);
        } else {
            marker = AFTER_GET_MARK_LVL(val_wr[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) <- Marker_Level=%d\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    marker);
        }
        c->do_reply = 1;
        return (0);
    }
    // For action rd_from_0 (read or write): publish results and return
    else if (action == 8) {
        if (action_type == 0) {
            marker = AFTER_GET_RD_FROM_0(val_rd[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) = 0x%x Read_From_0=%d\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    val_rd[0],
                    marker);
        } else {
            marker = AFTER_GET_RD_FROM_0(val_wr[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) <- Read_From_0=%d\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    marker);
        }
        c->do_reply = 1;
        return (0);
    }
    // For action test_digout (read or write): publish results and return
    else if (action == 9) {
        if (action_type == 0) {
            marker = AFTER_GET_TEST_DIGOUT(val_rd[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) = 0x%x Test_Digout=%d\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    val_rd[0],
                    marker);
        } else {
            marker = AFTER_GET_TEST_DIGOUT(val_wr[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) <- Test_Digout=%d\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    marker);
        }
        c->do_reply = 1;
        return (0);
    }
    // For action test_mode (read or write): publish results and return
    else if (action == 10) {
        if (action_type == 0) {
            test_mode = AFTER_GET_TEST_MODE(val_rd[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) = 0x%x Test_mode=%s\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    val_rd[0],
                    &After_Testmode2str[test_mode][0]);
        } else {
            test_mode = AFTER_GET_TEST_MODE(val_wr[0]);
            sprintf(rep, "0 Fem(%02d) After(%s) Reg(%d) <- Test_mode=%s\n",
                    c->fem->card_id,
                    &strr[0][0],
                    reg_adr,
                    &After_Testmode2str[test_mode][0]);
        }
        c->do_reply = 1;
        return (0);
    } else {
        err = ERR_ILLEGAL_PARAMETER;
        sprintf(rep, "%d Fem(%02d) After(%s) Reg(%d): action %s not implemented\n",
                err,
                c->fem->card_id,
                &strr[0][0],
                reg_adr,
                &strr[1][0]);
        c->do_reply = 1;
        return (err);
    }
}
