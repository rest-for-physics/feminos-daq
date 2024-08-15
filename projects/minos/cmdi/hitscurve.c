/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        hitscurve.c

 Description: Implementation of functions to work on AGET threshold S-curves


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  December 2011: created

  January 2014: corrected error on calculation of datagram size that prevented
  END_OF_FRAME to be sent

*******************************************************************************/

#include "hitscurve.h"
#include "error_codes.h"
#include "frame.h"
#include <stdio.h>

/*******************************************************************************
 FeminosShisto_Clear
*******************************************************************************/
int FeminosShisto_Clear(FeminosShisto* fs,
                        unsigned short a_beg,
                        unsigned short a_end,
                        unsigned short c_beg,
                        unsigned short c_end) {
    unsigned short i, j;
    int clr_cnt;

    clr_cnt = 0;
    for (i = a_beg; i <= a_end; i++) {
        for (j = c_beg; j <= c_end; j++) {
            Histo_Init(&(fs->asicshisto[i].shisto[j].s_histo), 0, SHISTO_SIZE - 1, 1, &(fs->asicshisto[i].shisto[j].s_bins[0]));
            clr_cnt++;
        }
    }

    return (clr_cnt);
}

/*******************************************************************************
 FeminosShisto_ClearAll
*******************************************************************************/
int FeminosShisto_ClearAll(FeminosShisto* fs) {
    fs->cur_thr = 0;

    return (FeminosShisto_Clear(fs, 0, (MAX_NB_OF_ASIC_PER_FEMINOS - 1), 0, ASIC_MAX_CHAN_INDEX));
}

/*******************************************************************************
 FeminosShisto_IntrepretCommand
*******************************************************************************/
int FeminosShisto_IntrepretCommand(CmdiContext* c) {
    int err;
    int param[10];
    char str[8][16];
    unsigned short asic_beg, asic_end;
    unsigned short cha_beg, cha_end;
    int arg_cnt;
    char* rep;

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME
    c->do_reply = 1;

    // Init
    asic_beg = 0;
    asic_end = 0;
    cha_beg = 0;
    cha_end = 0;
    err = 0;

    // Scan arguments
    if ((arg_cnt = sscanf(c->cmd, "shisto %s %s %s %s %s", &str[0][0], &str[1][0], &str[2][0], &str[3][0], &str[4][0])) >= 2) {
        // Wildcard on ASIC?
        if (str[1][0] == '*') {
            asic_beg = 0;
            asic_end = MAX_NB_OF_ASIC_PER_FEMINOS - 1;
        } else {
            // Scan ASIC index range
            if (sscanf(&str[1][0], "%d:%d", &param[0], &param[1]) == 2) {
                if (param[0] < param[1]) {
                    asic_beg = (unsigned short) param[0];
                    asic_end = (unsigned short) param[1];
                } else {
                    asic_beg = (unsigned short) param[1];
                    asic_end = (unsigned short) param[0];
                }
            }
            // Scan ASIC index
            else if (sscanf(&str[1][0], "%d", &param[0]) == 1) {
                asic_beg = (unsigned short) param[0];
                asic_end = (unsigned short) param[0];
            } else {
                err = ERR_SYNTAX;
            }
        }

        // Wildcard on Channel?
        if (str[2][0] == '*') {
            cha_beg = 0;
            cha_end = ASIC_MAX_CHAN_INDEX;
        } else {
            // Scan Channel index range
            if (sscanf(&str[2][0], "%d:%d", &param[0], &param[1]) == 2) {
                if (param[0] < param[1]) {
                    cha_beg = (unsigned short) param[0];
                    cha_end = (unsigned short) param[1];
                } else {
                    cha_beg = (unsigned short) param[1];
                    cha_end = (unsigned short) param[0];
                }
            }
            // Scan Channel index
            else if (sscanf(&str[2][0], "%d", &param[0]) == 1) {
                cha_beg = (unsigned short) param[0];
                cha_end = (unsigned short) param[0];
            } else {
                err = ERR_SYNTAX;
            }
        }
    }

    // Return if any syntax error was found
    if (err == ERR_SYNTAX) {
        sprintf(rep, "%d Fem(%02d) shisto <action> <ASIC> <Channel>\n", err, c->fem->card_id);
        return (err);
    }

    // Check that supplied arguments are within acceptable range
    if (!(
                (asic_beg < MAX_NB_OF_ASIC_PER_FEMINOS) &&
                (cha_beg <= ASIC_MAX_CHAN_INDEX) &&
                (asic_end < MAX_NB_OF_ASIC_PER_FEMINOS) &&
                (cha_end <= ASIC_MAX_CHAN_INDEX))) {
        err = ERR_ILLEGAL_PARAMETER;
        sprintf(rep, "%d Fem(%02d) shisto %s %d %d\n", err, c->fem->card_id, &str[0][0], asic_beg, cha_beg);
        return (err);
    }

    // See which action has to be done
    // Fill buffer with turn-on  histogram bins of multiple channels
    if (strncmp(&str[0][0], "getbins", 7) == 0) {
        err = 0;
        FeminosShisto_Packetize(c, asic_beg, asic_end, cha_beg, cha_end);
    }
    // Command to set which threshold is being accumulated in turn on histogram
    else if (strncmp(&str[0][0], "thr", 3) == 0) {

        if (sscanf(&str[3][0], "0x%x", &param[0]) == 1) {
            fshisto.cur_thr = param[0];
            err = 0;
            sprintf(rep, "%d Fem(%02d) thr <- 0x%x\n", err, c->fem->card_id, fshisto.cur_thr);
        } else {
            err = ERR_SYNTAX;
            sprintf(rep, "%d Fem(%02d) usage: thr <0xThr>\n", err, c->fem->card_id);
        }
    }
    // Clear histograms
    else if (strncmp(&str[0][0], "clr", 3) == 0) {
        if ((err = FeminosShisto_Clear(&fshisto, asic_beg, asic_end, cha_beg, cha_end)) < 0) {
            sprintf(rep, "%d Fem(%02d) shisto clr failed.\n", err, c->fem->card_id);
        } else {
            sprintf(rep, "0 Fem(%02d) shisto clr: cleared %d histograms.\n", err, c->fem->card_id);
            err = 0;
            ;
        }
    } else {
        err = ERR_SYNTAX;
        sprintf(rep, "%d Fem(%02d) usage: shisto <action> <asic> <channel>\n", err, c->fem->card_id);
    }
    return (err);
}

/*******************************************************************************
 FeminosShisto_Packetize
*******************************************************************************/
void FeminosShisto_Packetize(CmdiContext* c,
                             unsigned short as,
                             unsigned short ae,
                             unsigned short ch,
                             unsigned short ce) {
    int i;
    unsigned short size;
    unsigned short* p;
    unsigned short asic;
    unsigned short chan;

    size = 0;
    p = (unsigned short*) (c->burep);
    c->reply_is_cframe = 0;

    // Put Start Of Frame + Version
    *p = PUT_FVERSION_FEMID(PFX_START_OF_MFRAME, CURRENT_FRAMING_VERSION, c->fem->card_id);
    p++;
    size += 2;

    // Leave space for size;
    p++;
    size += 2;

    // Loop on desired ASICs
    for (asic = as; asic <= ae; asic++) {
        // Loop on desired channels
        for (chan = ch; chan <= ce; chan++) {
            // Put Card/Chip/Channel index for Histo
            *p = PUT_CARD_CHIP_CHAN_HISTO(c->fem->card_id, asic, chan);
            p++;
            size += 2;

            // Put indicator that hit S-curve histogram follows
            *p = PFX_SHISTO_BINS;
            p++;
            size += 2;

            // Put hit S-curve histogram bins
            for (i = 0; i < 16; i++) {
                *p = fshisto.asicshisto[asic].shisto[chan].s_bins[i];
                p++;
                size += 2;
            }
        }
    }

    // Put End Of Frame
    *p = PFX_END_OF_FRAME;
    p++;
    size += 2;

    // Put packet size after start of frame
    p = (unsigned short*) (c->burep);
    p++;
    *p = size;

    // Include in the size the 2 empty bytes kept at the beginning of the UDP datagram
    size += 2;
    c->rep_size = size;
}

/*******************************************************************************
 FeminosShisto_UpdateHisto
*******************************************************************************/
void FeminosShisto_UpdateHisto(FeminosShisto* fs, unsigned short asic, unsigned short channel) {
#ifdef HAS_SHISTOGRAMS

    // Update histogram
    if ((asic < MAX_NB_OF_ASIC_PER_FEMINOS) && (channel <= ASIC_MAX_CHAN_INDEX)) {
        Histo_AddEntry(&(fs->asicshisto[asic].shisto[channel].s_histo), fs->cur_thr);
    }

#endif
}

/*******************************************************************************
 FeminosShisto_ScanDFrame
*******************************************************************************/
int FeminosShisto_ScanDFrame(void* fr, int sz) {
#ifdef HAS_SHISTOGRAMS
    unsigned short* p;
    int i, j;
    int sz_rd;
    int done = 0;
    unsigned short r0, r1, r2;
    int si;
    char* c;
    unsigned int* ui;
    int err;

    p = (unsigned short*) fr;

    done = 0;
    i = 0;
    sz_rd = 0;
    si = 0;

    r0 = 0;
    r1 = 0;
    r2 = 0;

    err = 0;

    while (!done) {
        // Is it a prefix for 14-bit content?
        if ((*p & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_IX) {
            r0 = GET_CARD_IX(*p);
            r1 = GET_CHIP_IX(*p);
            r2 = GET_CHAN_IX(*p);
            // printf("Card %02d Chip %01d Channel %02d\n", r0, r1, r2);
            FeminosShisto_UpdateHisto(&fshisto, r1, r2);
            i++;
            p++;
            sz_rd += 2;
            si = 0;
        } else if ((*p & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_CNT) {
            i++;
            p++;
            sz_rd += 2;
        } else if ((*p & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HISTO) {
            i++;
            p++;
            sz_rd += 2;
        }
        // Is it a prefix for 12-bit content?
        else if ((*p & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE) {
            r0 = GET_ADC_DATA(*p);
            i++;
            p++;
            sz_rd += 2;
            si++;
        } else if ((*p & PFX_12_BIT_CONTENT_MASK) == PFX_LAT_HISTO_BIN) {
            i++;
            p++;
            sz_rd += 2;
            r1 = *p;
            i++;
            p++;
            sz_rd += 2;
            r2 = *p;
            i++;
            p++;
            sz_rd += 2;
        }
        // Is it a prefix for 9-bit content?
        else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_TIME_BIN_IX) {
            i++;
            p++;
            sz_rd += 2;
            si = 0;
        } else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_HISTO_BIN_IX) {
            i++;
            p++;
            sz_rd += 2;
        } else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME) {
            i++;
            p++;
            sz_rd += 2;
            i++;
            p++;
            sz_rd += 2;
        } else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_MFRAME) {
            i++;
            p++;
            sz_rd += 2;
            i++;
            p++;
            sz_rd += 2;
        } else if ((*p & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_CFRAME) {
            i++;
            p++;
            sz_rd += 2;
            i++;
            p++;
            sz_rd += 2;
        }
        // Is it a prefix for 8-bit content?
        else if ((*p & PFX_8_BIT_CONTENT_MASK) == PFX_ASCII_MSG_LEN) {
            r0 = GET_ASCII_LEN(*p);
            i++;
            p++;
            sz_rd += 2;
            c = (char*) p;
            for (j = 0; j < r0; j++) {
                c++;
            }
            // Skip the null string terminating character
            r0++;
            // But if the resulting size is odd, there is another null character that we should skip
            if (r0 & 0x0001) {
                r0++;
            }
            p += (r0 >> 1);
            i += (r0 >> 1);
            sz_rd += r0;
        }
        // Is it a prefix for 4-bit content?
        else if ((*p & PFX_4_BIT_CONTENT_MASK) == PFX_START_OF_EVENT) {
            i++;
            p++;
            sz_rd += 2;

            // Time Stamp lower 16-bit
            i++;
            p++;
            sz_rd += 2;

            // Time Stamp middle 16-bit
            i++;
            p++;
            sz_rd += 2;

            // Time Stamp upper 16-bit
            i++;
            p++;
            sz_rd += 2;

            // Event Count lower 16-bit
            i++;
            p++;
            sz_rd += 2;

            // Event Count upper 16-bit
            i++;
            p++;
            sz_rd += 2;
        } else if ((*p & PFX_4_BIT_CONTENT_MASK) == PFX_END_OF_EVENT) {
            i++;
            p++;
            sz_rd += 2;
            i++;
            p++;
            sz_rd += 2;
        }
        // Is it a prefix for 2-bit content?

        // Is it a prefix for 0-bit content?
        else if ((*p & PFX_0_BIT_CONTENT_MASK) == PFX_END_OF_FRAME) {
            i++;
            p++;
            sz_rd += 2;
        } else if (*p == PFX_NULL_CONTENT) {
            i++;
            p++;
            sz_rd += 2;
        } else if (*p == PFX_DEADTIME_HSTAT_BINS) {
            p++;
            i++;
            sz_rd += 2;
            p++;
            i++;
            sz_rd += 2;

            ui = (unsigned int*) p;

            ui++;
            i += 2;
            sz_rd += 4;

            ui++;
            i += 2;
            sz_rd += 4;

            ui++;
            i += 2;
            sz_rd += 4;

            ui++;
            i += 2;
            sz_rd += 4;

            ui++;
            i += 2;
            sz_rd += 4;

            ui++;
            i += 2;
            sz_rd += 4;

            ui++;
            i += 2;
            sz_rd += 4;

            ui++;
            i += 2;
            sz_rd += 4;

            ui++;
            i += 2;
            sz_rd += 4;

            // Save last value of pointer
            p = (unsigned short*) ui;
        }
        // No interpretable data
        else {
            sz_rd += 2;
            p++;
        }

        // Check if end of packet was reached
        if (sz_rd == sz) {
            done = 1;
        } else if (sz_rd > sz) {
            err = -1;
            printf("Format error: read %d bytes but packet size is %d\n", sz_rd, sz);
            done = 1;
        }
    }
    return (err);
#endif
}
