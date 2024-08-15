/*******************************************************************************

                           _____________________

 File:        pulser.c

 Description: FEC DAC Pulser Programming from FEMINOS.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr

 History:
  May 2011: created

*******************************************************************************/

#include "pulser.h"
#include "error_codes.h"

#include <stdio.h>
#include <string.h>

#define MAX_SC_REQ_RETRY 100

/*******************************************************************************
 Pulser_SetAmplitude
*******************************************************************************/
int Pulser_SetAmplitude(Feminos* fem, unsigned short amp) {
    int i;
    unsigned short dac_val;
    int err = 0;
    unsigned int dat_wr, dat_rd;
    int retry;

    // Request access to slow control lines
    dat_wr = R1_PUT_SC_REQ(0, 1);
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_SC_REQ, &dat_wr)) < 0) {
        return (err);
    }

    // Wait for access grant to slow control lines
    retry = 0;
    do {
        if ((err = Feminos_ActOnRegister(fem, FM_REG_READ, 1, R1_SC_GRANT, &dat_rd)) < 0) {
            return (err);
        } else {
            if (retry > MAX_SC_REQ_RETRY) {
                return (ERR_SC_NOT_GRANTED);
            } else {
                retry++;
            }
        }
    } while (!R1_GET_SC_GRANT(dat_rd));

    // De-activate serial clock, serial data and pulser DAC chip select in case any was left over
    dat_wr = 0;
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, (R1_ASFC_SCLK | R1_ASFC_MOSI | R1_GEN_CS), &dat_wr)) < 0) {
        return (err);
    }

    // loop 16 times for serial write
    dac_val = amp;
    for (i = 0; i < 16; i++) {

        // get the bit from the value to be written to the DAC
        if (dac_val & 0x8000) {
            dat_wr = R1_ASFC_MOSI;
        } else {
            dat_wr = 0;
        }
        dac_val = dac_val << 1;

        // Put the bit of serial data at the input of the pulser DAC and reset serial clock
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, (R1_ASFC_MOSI | R1_ASFC_SCLK), &dat_wr)) < 0) {
            return (err);
        }

        // Set the serial clock to 1 without changing the serial data
        dat_wr = dat_wr | R1_ASFC_SCLK;
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, (R1_ASFC_MOSI | R1_ASFC_SCLK), &dat_wr)) < 0) {
            return (err);
        }
    }

    // Reset the serial clock without changing the serial data
    dat_wr = dat_wr & (~R1_ASFC_SCLK);
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, (R1_ASFC_MOSI | R1_ASFC_SCLK), &dat_wr)) < 0) {
        return (err);
    }

    // activate pulser DAC chip select to load pulser shift register
    dat_wr = dat_wr | R1_GEN_CS;
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, (R1_ASFC_MOSI | R1_ASFC_SCLK | R1_GEN_CS), &dat_wr)) < 0) {
        return (err);
    }

    // de-activate pulser DAC chip select and serial data
    dat_wr = 0;
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, (R1_ASFC_MOSI | R1_GEN_CS), &dat_wr)) < 0) {
        return (err);
    }

    // Release the slow control lines
    dat_wr = R1_PUT_SC_REQ(0, 0);
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_SC_REQ, &dat_wr)) < 0) {
        return (err);
    }

    return (err);
}
