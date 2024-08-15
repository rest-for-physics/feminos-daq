/*******************************************************************************

                           _____________________

 File:        aget.c

 Description: functions to control AGET chip with Feminos readout electronics.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  September 2011: created

*******************************************************************************/
#include "aget.h"
#include "error_codes.h"

#define MAX_SC_REQ_RETRY 100

/*******************************************************************************
 AgetChannelNumber2DaqChannelIndex
 Converts the index of the physical channels of the AGET chip which run from
 1 to 64, to the channel index numbering used by the DAQ. This numbering
 includes the 4 FPN channels and the RESET channels which can be 2 or 4
 depending on the setting of the AGET chip.
*******************************************************************************/
unsigned short AgetChannelNumber2DaqChannelIndex(unsigned short cha_num, unsigned short rst_len) {
    unsigned short daq_cha_num;

    // Short read sequence: DAQ channel index range from 0 to 70 (2 RESET + 64 channels + 4 FPN)
    if (cha_num <= 11) {
        daq_cha_num = cha_num + 1;
    } else if (cha_num <= 21) {
        daq_cha_num = cha_num + 2;
    } else if (cha_num <= 43) {
        daq_cha_num = cha_num + 3;
    } else if (cha_num <= 53) {
        daq_cha_num = cha_num + 4;
    } else {
        daq_cha_num = cha_num + 5;
    }

    // Long read sequence: DAQ channel index range from 0 to 72 (4 RESET + 64 channels + 4 FPN)
    if (rst_len == 1) {
        daq_cha_num += 2;
    }
    return (daq_cha_num);
}

/*******************************************************************************
 Aget_SetControlMode
*******************************************************************************/
int Aget_SetControlMode(Feminos* fem, unsigned short aget, unsigned short op) {
    int i;
    int err = 0;
    unsigned int asic_sel;
    unsigned int dat_wr;

    // Put the serial clock to 1 and serial data to 0
    dat_wr = R1_PUT_ASFC_SCLK(0, 1);
    dat_wr = R1_PUT_ASFC_MOSI(dat_wr, 0);
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, (R1_ASFC_MOSI | R1_ASFC_SCLK), &dat_wr)) < 0) {
        return (err);
    }

    // Write specific pattern to prepare configuration of AGET control mode
    for (i = 0; i < 3; i++) {
        // activate chip select on the selected ASIC
        asic_sel = R1_PUT_ASFC_CS(0, (1 << aget));
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_CS_M, &asic_sel)) < 0) {
            return (err);
        }

        // Put the serial clock to 0
        dat_wr = R1_PUT_ASFC_SCLK(0, 0);
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_SCLK, &dat_wr)) < 0) {
            return (err);
        }

        // Put the serial clock to 1
        dat_wr = R1_PUT_ASFC_SCLK(0, 1);
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_SCLK, &dat_wr)) < 0) {
            return (err);
        }

        // de-activate chip select on the selected ASIC
        asic_sel = R1_PUT_ASFC_CS(0, 0);
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_CS_M, &asic_sel)) < 0) {
            return (err);
        }

        // Put Data in to 1 or 0 depending on op
        if (i == 2) {
            dat_wr = R1_PUT_ASFC_MOSI(0, op);
            if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_MOSI, &dat_wr)) < 0) {
                return (err);
            }
        }

        // Put the serial clock to 0
        dat_wr = R1_PUT_ASFC_SCLK(0, 0);
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_SCLK, &dat_wr)) < 0) {
            return (err);
        }

        // Put the serial clock to 1
        dat_wr = R1_PUT_ASFC_SCLK(0, 1);
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_SCLK, &dat_wr)) < 0) {
            return (err);
        }
    }

    // Set serial data to 0
    dat_wr = R1_PUT_ASFC_MOSI(0, 0);
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_MOSI, &dat_wr)) < 0) {
        return (err);
    }

    // Put the serial clock to 0
    dat_wr = R1_PUT_ASFC_SCLK(0, 0);
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_SCLK, &dat_wr)) < 0) {
        return (err);
    }

    // Make a couple of extra serial clock cycles to be sure we are done
    for (i = 0; i < 2; i++) {
        // Put the serial clock to 1
        dat_wr = R1_PUT_ASFC_SCLK(0, 1);
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_SCLK, &dat_wr)) < 0) {
            return (err);
        }

        // Put the serial clock to 0
        dat_wr = R1_PUT_ASFC_SCLK(0, 0);
        if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_SCLK, &dat_wr)) < 0) {
            return (err);
        }
    }

    return (err);
}

/*******************************************************************************
 Aget_ReadWrite
*******************************************************************************/
int Aget_ReadWrite(Feminos* fem, unsigned short aget, unsigned short adr, unsigned short* val, unsigned short op) {
    unsigned short mask;
    int err;
    int i;
    int nb_loops, dis_cs_at;
    unsigned short* dat_s;
    unsigned int asic_sel;
    unsigned int dat_wr, dat_rd;
    int retry;
    unsigned short aget_mode;

    // Check that the FEM is in the AGET mode
    if ((err = Feminos_ActOnRegister(fem, FM_REG_READ, 0, 0xFFFFFFFF, &dat_rd)) < 0) {
        return (err);
    } else {
        // Cannot work on AGET chip if we are in AFTER mode
        if (R0_GET_MODE(dat_rd) == 1) {
            return (ERR_ILLEGAL_PARAMETER);
        }
    }

    // Check parameters
    if ((aget > (MAX_NB_OF_AGET_PER_FEMINOS - 1)) || (adr > AGET_REGISTER_MAX_ADDRESS)) {
        return (ERR_ILLEGAL_PARAMETER);
    }

    // Clear output result if read
    if (op == 1) // Read
    {
        if (adr == 0) // maps to hit register and is assumed to be 68-bit
        {
            *val = 0x0000;                   // bit 79-64
            *(val + 1) = 0x0000;             // bit 63-48
            *(val + 2) = 0x0000;             // bit 47-32
            *(val + 3) = 0x0000;             // bit 31-16
            *(val + 4) = 0x0000;             // bit 15-0
        } else if ((adr == 1) || (adr == 2)) // 32-bit registers
        {
            *val = 0x0000;                   // bit 31-16
            *(val + 1) = 0x0000;             // bit 15-0
        } else if ((adr == 3) || (adr == 4)) // 34-bit registers
        {
            *val = 0x0000;                    // bit 33-32
            *(val + 1) = 0x0000;              // bit 31-16
            *(val + 2) = 0x0000;              // bit 15-0
        } else if ((adr == 5) || (adr == 12)) // 16-bit registers
        {
            *val = 0x0000;                                                 // bit 15-0
        } else if ((adr == 6) || (adr == 7) || (adr == 10) || (adr == 11)) // 64-bit registers
        {
            *val = 0x0000;                   // bit 63-48
            *(val + 1) = 0x0000;             // bit 47-32
            *(val + 2) = 0x0000;             // bit 31-16
            *(val + 3) = 0x0000;             // bit 15-0
        } else if ((adr == 8) || (adr == 9)) // 128-bit registers
        {
            *val = 0x0000;       // bit 127-112
            *(val + 1) = 0x0000; // bit 111-96
            *(val + 2) = 0x0000; // bit 95-80
            *(val + 3) = 0x0000; // bit 79-64
            *(val + 4) = 0x0000; // bit 63-48
            *(val + 5) = 0x0000; // bit 47-32
            *(val + 6) = 0x0000; // bit 31-16
            *(val + 7) = 0x0000; // bit 15-0
        } else {
            return (ERR_ILLEGAL_PARAMETER);
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

    // Set AGET control mode to "slow control mode" or "channel address control mode"
    if (adr == 0) {
        aget_mode = CHANNEL_ACCESS_MODE;
    } else {
        aget_mode = SLOWCTRL_ACCESS_MODE;
    }
    if ((err = Aget_SetControlMode(fem, aget, aget_mode)) < 0) {
        return (err);
    }

    // Now we can start the read/write operation to an AGET register

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
    asic_sel = R1_PUT_ASFC_CS(0, (1 << aget));

    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_ASFC_CS_M, &asic_sel)) < 0) {
        return (err);
    }

    if (aget_mode == SLOWCTRL_ACCESS_MODE) {
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
    }

    // Determine the number of loops required to read or write the variable length data
    if (adr == 0) // maps to hit register and is assumed to be 68-bit for write 69-bit for read
    {
        if (op == 0) // Write
        {
            nb_loops = 70;
            dis_cs_at = 68;
            mask = 0x0008;
        } else {
            nb_loops = 70;
            dis_cs_at = 70;
            mask = 0x0010;
        }
    } else if ((adr == 1) || (adr == 2)) // 32-bit registers
    {
        if (op == 0) // Write
        {
            nb_loops = 32 + 4;
            dis_cs_at = 32;
        } else {
            nb_loops = 34;
            dis_cs_at = 34 - 4;
        }
        mask = 0x8000;
    } else if ((adr == 3) || (adr == 4)) // 34-bit registers
    {
        if (op == 0) // Write
        {
            nb_loops = 34 + 4;
            dis_cs_at = 34;
        } else {
            nb_loops = 36;
            dis_cs_at = 36 - 4;
        }
        mask = 0x0002;
    } else if ((adr == 5) || (adr == 12)) // 16-bit registers
    {
        if (op == 0) // Write
        {
            nb_loops = 16 + 4;
            dis_cs_at = 16;
        } else {
            nb_loops = 18;
            dis_cs_at = 18 - 4;
        }
        mask = 0x8000;
    } else if ((adr == 6) || (adr == 7) || (adr == 10) || (adr == 11)) // 64-bit registers
    {
        if (op == 0) // Write
        {
            nb_loops = 64 + 4;
            dis_cs_at = 64;
        } else {
            nb_loops = 66;
            dis_cs_at = 66 - 4;
        }
        mask = 0x8000;
    } else if ((adr == 8) || (adr == 9)) // 128-bit registers
    {
        if (op == 0) // Write
        {
            nb_loops = 128 + 4;
            dis_cs_at = 128;
        } else {
            nb_loops = 130;
            dis_cs_at = 130 - 4;
        }
        mask = 0x8000;
    } else {
        return (ERR_ILLEGAL_PARAMETER);
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
            dat_rd = dat_rd & (1 << aget);
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

    // We are done with the read/write at this stage

    // Set AGET control mode to "channel address control mode" in all cases at the end
    if ((err = Aget_SetControlMode(fem, aget, CHANNEL_ACCESS_MODE)) < 0) {
        return (err);
    }

    // Release the slow control lines
    dat_wr = R1_PUT_SC_REQ(0, 0);
    if ((err = Feminos_ActOnRegister(fem, FM_REG_WRITE, 1, R1_SC_REQ, &dat_wr)) < 0) {
        return (err);
    }

    // Now we have to update the mirror of registers
    if ((adr == 1) || (adr == 2)) // 32-bit registers
    {
        fem->asics[aget].bar[adr].bars[0] = *(val + 1);
        fem->asics[aget].bar[adr].bars[1] = *(val + 0);
    } else if ((adr == 3) || (adr == 4)) // 34-bit registers
    {
        fem->asics[aget].bar[adr].bars[0] = *(val + 2);
        fem->asics[aget].bar[adr].bars[1] = *(val + 1);
        fem->asics[aget].bar[adr].bars[2] = *(val + 0);
    } else if ((adr == 5) || (adr == 12)) // 16-bit registers
    {
        fem->asics[aget].bar[adr].bars[0] = *val;
    } else if ((adr == 6) || (adr == 7) || (adr == 10) || (adr == 11)) // 64-bit registers
    {
        fem->asics[aget].bar[adr].bars[0] = *(val + 3);
        fem->asics[aget].bar[adr].bars[1] = *(val + 2);
        fem->asics[aget].bar[adr].bars[2] = *(val + 1);
        fem->asics[aget].bar[adr].bars[3] = *(val + 0);
    } else if ((adr == 8) || (adr == 9)) // 128-bit registers
    {
        fem->asics[aget].bar[adr].bars[0] = *(val + 7);
        fem->asics[aget].bar[adr].bars[1] = *(val + 6);
        fem->asics[aget].bar[adr].bars[2] = *(val + 5);
        fem->asics[aget].bar[adr].bars[3] = *(val + 4);
        fem->asics[aget].bar[adr].bars[4] = *(val + 3);
        fem->asics[aget].bar[adr].bars[5] = *(val + 2);
        fem->asics[aget].bar[adr].bars[6] = *(val + 1);
        fem->asics[aget].bar[adr].bars[7] = *(val + 0);
    }

    return (err);
}

/*******************************************************************************
 Aget_VerifiedWrite
*******************************************************************************/
int Aget_VerifiedWrite(Feminos* fem, unsigned short aget, unsigned short adr, unsigned short* val) {
    int err;
    unsigned short val_rd[8];
    int i, nb_sho;

    // Determine the number of short words that will be written
    if (adr == 0) // maps to hit register and is assumed to be 68-bit
    {
        nb_sho = 5;
    } else if ((adr == 1) || (adr == 2)) // 32-bit registers
    {
        nb_sho = 2;
    } else if ((adr == 3) || (adr == 4)) // 34-bit registers
    {
        nb_sho = 3;
    } else if ((adr == 5) || (adr == 12)) // 16-bit registers
    {
        nb_sho = 1;
    } else if ((adr == 6) || (adr == 7) || (adr == 10) || (adr == 11)) // 64-bit registers
    {
        nb_sho = 4;
    } else if ((adr == 8) || (adr == 9)) // 128-bit registers
    {
        nb_sho = 8;
    } else {
        return (ERR_ILLEGAL_PARAMETER);
    }

    // Perform write
    if ((err = Aget_ReadWrite(fem, aget, adr, val, 0)) < 0) {
        return (err);
    } else {
        // Perform read
        if ((err = Aget_ReadWrite(fem, aget, adr, &val_rd[0], 1)) < 0) {
            return (err);
        } else {
            // Make comparisons
            for (i = 0; i < nb_sho; i++) {
                if (val_rd[i] != *(val + i)) {
                    return (ERR_VERIFY_MISMATCH);
                }
            }
            return (0);
        }
    }
}

/*******************************************************************************
 Aget_Write
*******************************************************************************/
int Aget_Write(Feminos* fem, unsigned short aget, unsigned short adr, unsigned short* val) {
    return (Aget_ReadWrite(fem, aget, adr, val, 0));
}

/*******************************************************************************
 Aget_Read
*******************************************************************************/
int Aget_Read(Feminos* fem, unsigned short aget, unsigned short adr, unsigned short* val) {
    return (Aget_ReadWrite(fem, aget, adr, val, 1));
}
