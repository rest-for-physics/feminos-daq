/*******************************************************************************

                           _____________________

 File:        after.c

 Description:

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  September 2011 : adapted from T2K code


*******************************************************************************/

#include "after.h"
#include "error_codes.h"

#define MAX_SC_REQ_RETRY 100

/*******************************************************************************
 After_ReadWrite
*******************************************************************************/
int After_ReadWrite(Feminos* fem, unsigned short asic, unsigned short adr, unsigned short* val, unsigned short op) {
    unsigned short mask;
    int err;
    int i;
    int nb_loops, dis_cs_at;
    unsigned short* dat_s;
    unsigned int asic_sel;
    unsigned int dat_wr, dat_rd;
    int retry;

    // Check that the FEM is in the AFTER mode
    if ((err = Feminos_ActOnRegister(fem, FM_REG_READ, 0, 0xFFFFFFFF, &dat_rd)) < 0) {
        return (err);
    } else {
        // Cannot work on AFTER chip if we are in AGET mode
        if (R0_GET_MODE(dat_rd) == 0) {
            return (ERR_ILLEGAL_PARAMETER);
        }
    }

    // Check parameters
    if ((asic > (MAX_NB_OF_AFTER_PER_FEMINOS - 1)) || (adr > ASIC_REGISTER_MAX_ADDRESS)) {
        return (ERR_ILLEGAL_PARAMETER);
    }

    // Clear output result if read
    if (op == 1) // Read
    {
        *val = 0x0000; // bit 15-0 or 37-32
        if ((adr == 3) || (adr == 4)) {
            *(val + 1) = 0x0000; // bit 31-16
            *(val + 2) = 0x0000; // bit 15-0
        }
    }

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

    // Prepare the read/write bit on the serial data input of the ASIC
    if (op == 0) // Write
    {
        dat_wr = R1_PUT_ASFC_SCLK(0, 1);
    } else // Read
    {
        dat_wr = R1_PUT_ASFC_SCLK(0, 1);
        dat_wr = R1_PUT_ASFC_MOSI(dat_wr, 1);
    }

    // Put the serial clock to 1 and put the read/write bit
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, (R1_ASFC_MOSI | R1_ASFC_SCLK), &dat_wr)) < 0) {
        return (err);
    }

    // activate chip select on the selected ASIC
    asic_sel = R1_PUT_ASFC_CS(0, (1 << asic));

    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_CS_M, &asic_sel)) < 0) {
        return (err);
    }

    // Load the 7 address bits serially, MSB first
    mask = 0x0040;
    for (i = 0; i < 7; i++) {
        // Set the serial clock low without changing the serial data
        dat_wr = R1_PUT_ASFC_SCLK(0, 0);
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_SCLK, &dat_wr)) < 0) {
            return (err);
        }

        // Compute next bit of serial data
        if (adr & mask) {
            dat_wr = R1_PUT_ASFC_SCLK(0, 1);
            dat_wr = R1_PUT_ASFC_MOSI(dat_wr, 1);
        } else {
            dat_wr = R1_PUT_ASFC_SCLK(0, 1);
        }
        mask >>= 1;

        // set SCLK high and place the next bit of serial data
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, (R1_ASFC_MOSI | R1_ASFC_SCLK), &dat_wr)) < 0) {
            return (err);
        }
    }

    // Determine the number of loops required to read or write the variable length data
    if ((adr == 3) || (adr == 4)) {
        if (op == 0) // Write
        {
            nb_loops = 38 + 4;
            dis_cs_at = 38;
        } else {
            nb_loops = 40;
            dis_cs_at = 40 - 4;
        }
        mask = 0x0020;
    } else {
        if (op == 0) // Write
        {
            nb_loops = 16 + 4;
            dis_cs_at = 16;
        } else {
            nb_loops = 18;
            dis_cs_at = 18 - 4;
        }
        mask = 0x8000;
    }

    // Load serial data for write or shift out data for read
    dat_s = val;
    for (i = 0; i < nb_loops; i++) {
        // Set the serial clock low without changing the serial data
        dat_wr = R1_PUT_ASFC_SCLK(0, 0);
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_SCLK, &dat_wr)) < 0) {
            return (err);
        }

        // de-activate chip select on all ASICs
        if (i == dis_cs_at) {
            asic_sel = R1_PUT_ASFC_CS(0, 0);
            if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_CS_M, &asic_sel)) < 0) {
                return (err);
            }
        }

        // Capture the serial data output in case of a read
        // but skip the first bit shifted out of the serial link
        if ((op == 1) && (i > 0)) {
            if ((err = Feminos_ActOnRegister(fem, FM_REG_READ, 1, R1_ASFC_MISO_M, &dat_rd)) < 0) {
                return (err);
            }
            dat_rd = R1_GET_ASFC_MISO(dat_rd);
            dat_rd = dat_rd & (1 << asic);
            if (dat_rd) // the bit read was 1
            {
                *dat_s = *dat_s | mask;
            }
            if (mask == 1) {
                mask = 0x8000;
                dat_s++;
            } else {
                mask >>= 1;
            }
        }

        // Compute next bit of serial data
        if (op == 1) // read
        {
            dat_wr = R1_PUT_ASFC_SCLK(0, 1);
        } else // write
        {
            if (i < dis_cs_at) // get data bit from supplied parameter
            {
                if (*dat_s & mask) {
                    dat_wr = R1_PUT_ASFC_SCLK(0, 1);
                    dat_wr = R1_PUT_ASFC_MOSI(dat_wr, 1);
                } else {
                    dat_wr = R1_PUT_ASFC_SCLK(0, 1);
                }
                if (mask == 1) {
                    mask = 0x8000;
                    dat_s++;
                } else {
                    mask >>= 1;
                }
            } else // no more data bits to read from parameter (4 trailer serial clock ticks)
            {
                dat_wr = R1_PUT_ASFC_SCLK(0, 1);
            }
        }

        // set SCLK high and place the next bit of serial data
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, (R1_ASFC_MOSI | R1_ASFC_SCLK), &dat_wr)) < 0) {
            return (err);
        }
    }

    // Set the serial clock low without changing the serial data
    dat_wr = R1_PUT_ASFC_SCLK(0, 0);
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_SCLK, &dat_wr)) < 0) {
        return (err);
    }

    // Release the slow control lines
    dat_wr = R1_PUT_SC_REQ(0, 0);
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_SC_REQ, &dat_wr)) < 0) {
        return (err);
    }

    // Now we have to update the mirror of registers
    if ((adr == 1) || (adr == 2)) // 16-bit registers
    {
        fem->asics[asic].bar[adr].bars[0] = *(val + 0);
    } else if ((adr == 3) || (adr == 4)) // 38-bit registers
    {
        fem->asics[asic].bar[adr].bars[0] = *(val + 2);
        fem->asics[asic].bar[adr].bars[1] = *(val + 1);
        fem->asics[asic].bar[adr].bars[2] = *(val + 0);
    }

    return (err);
}

/*******************************************************************************
 After_Write
*******************************************************************************/
int After_Write(Feminos* fem, unsigned short asic, unsigned short adr, unsigned short* val) {
    return (After_ReadWrite(fem, asic, adr, val, 0));
}

/*******************************************************************************
 After_Read
*******************************************************************************/
int After_Read(Feminos* fem, unsigned short asic, unsigned short adr, unsigned short* val) {
    return (After_ReadWrite(fem, asic, adr, val, 1));
}
