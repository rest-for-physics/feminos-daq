/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        ds2438.c

 Description: Implementation of functions for controlling a DS2438.
  Platform independant implementation

 Author:      X. de la Bro�se,        labroise@cea.fr
              D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  May 2007: created

*******************************************************************************/

#include "ds2438.h"
#include "ds2438_api.h"

//--------------------------------------------------------------------------
// Send 8 bits of communication to the 1-Wire Net
//
// dev      a pointer to the device
// portnum  This number is provided to indicate the symbolic port number.
// sendbyte 8 bits to send
//
// Returns:  0  : success
//           <0 : failed
//
int ow_WriteByte(void* dev, int portnum, unsigned char sendbyte) {
    int loop;
    int err;

    // Loop to write each bit in the byte, LS-bit first
    for (loop = 0; loop < 8; loop++) {
        if ((err = ow_WriteBit(dev, portnum, sendbyte)) < 0) {
            return (err);
        } else {
            // shift the data byte for the next bit
            sendbyte >>= 1;
        }
    }
    return (0);
}

//--------------------------------------------------------------------------
// Send 8 bits of read communication to the 1-Wire Net and return the
// result 8 bits read from the 1-Wire Net.
//
// dev      a pointer to the device
// portnum  This number is provided to indicate the symbolic port number.
// rcvbyte  the byte read
//
// Returns:  0  : success
//           <0 : failed
//
int ow_ReadByte(void* dev, int portnum, unsigned char* rcvbyte) {
    int loop;
    unsigned char result;
    int err;

    *rcvbyte = 0;
    for (loop = 0; loop < 8; loop++) {
        // shift the result to get it ready for the next bit
        *rcvbyte >>= 1;

        // read bit
        if ((err = ow_ReadBit(dev, portnum, &result)) < 0) {
            return (err);
        } else {
            if (result) {
                *rcvbyte |= 0x80;
            }
        }
    }

    return (err);
}

/**
 * Performs a Reset/Presence on the DS2438 followed by
 * a Skip ROM and a series of Memory/Control commands
 *
 * dev      a pointer to the device
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * cmdcnt    command count
 * cmd       an array of commands
 *
 *
 * Return :   0 : success
 *          < 0 : failed
 */
int ow_SendCommands(void* dev, int portnum, int cmdcnt, unsigned char* cmd) {
    int i;
    int err;

    // Make RESET/Presence
    if ((err = ow_TouchReset(dev, portnum)) < 0) {
        return (err);
    }

    // Send Skip ROM command
    if ((err = ow_WriteByte(dev, portnum, OW_SKIP_ROM)) < 0) {
        return (err);
    }

    // Send command array
    for (i = 0; i < cmdcnt; i++) {
        if ((err = ow_WriteByte(dev, portnum, *(cmd + i))) < 0) {
            return (err);
        }
    }

    return (0);
}

/**
 * Check the CRC of a block of data from the DS2438.
 * The last byte supplied is the CRC computed by the DS2438
 *
 * blk     the block to be checked. Type : unsigned char[len+1].
 * len     length of the block excluding CRC
 *
 * Return :   0 : success
 *          < 0 : failed
 */
int ow_CheckCRC(unsigned char* blk, int len) {
    int i, j;
    unsigned char crc, lsb, msk;

    // Loop on all bytes starting from least significant byte
    crc = 0;
    for (i = 0; i < len; i++) {
        // Loop on all bits within a byte starting from LSB
        msk = *(blk + i);
        for (j = 0; j < 8; j++) {
            lsb = (crc & 0x1) ^ (msk & 0x1);
            crc = crc >> 1;
            if (lsb) {
                crc ^= 0x8C;
            }
            msk = msk >> 1;
        }
    }

    // check the final crc
    if (crc == *(blk + len)) {
        return (0);
    } else {
        return (OWERROR_INCORRECT_CRC);
    }
}

/**
 * Reads the serial number from the DS2438.
 *
 * dev      a pointer to the device
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * SNum     the serial number that has been read. Type : unsigned char[8].
 *          *Snum     = 0x26 Family code
 *          *(Snum+1) = Less Significant Byte of 48-bit serial number
 *          *(Snum+6) = Most Significant Byte of 48-bit serial number
 *          *(Snum+7) = 8-bit CRC Code
 * Return :   0 : success
 *          < 0 : failed
 */
int ow_GetSerialNum(void* dev, int portnum, unsigned char* SNum) {
    int i;
    unsigned char* rdbyte;
    int err;

    // Make RESET/Presence
    if ((err = ow_TouchReset(dev, portnum)) < 0) {
        return (err);
    }

    // Send Read ROM command
    if ((err = ow_WriteByte(dev, portnum, OW_READ_ROM)) < 0) {
        return (err);
    }

    // Send 8 Read Byte commands
    rdbyte = SNum;
    for (i = 0; i < 8; i++) {
        if ((err = ow_ReadByte(dev, portnum, rdbyte)) < 0) {
            return (err);
        }
        rdbyte++;
    }

    return (ow_CheckCRC(SNum, 7));
}

/**
 * Reads the temperature, voltage or current from the DS2438.
 *
 * dev      a pointer to the device
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 *
 * op       convertion operation to perform
 * cv	      return the converted value:
 *    - temperature: multiply by 0.03125 to obtain �C
 *    - voltage: multiply by 0.01 to obtain V
 *    - current: multiply by Rsense / 4096 to obtain A
 *
 * Return :   0 : success
 *          < 0 : failed
 */
int ow_GetConversion(void* dev, int op, int portnum, short* cv) {
    int i;
    int retry;
    int err;
    unsigned char cmd[4];
    unsigned char rd_bit;
    unsigned char rd_bytes[9];

    // Send command corresponding to desired conversion
    if (op == OW_CONV_TEMP) {
        // Send Convert Temperature command
        cmd[0] = OW_CONVERT_T;
        if ((err = ow_SendCommands(dev, portnum, 1, &cmd[0])) < 0) {
            return (err);
        }
    } else if ((op == OW_CONV_VDD) || (op == OW_CONV_VAD)) {
        // Send Write Scratch Pad Page 0x00 command
        cmd[0] = OW_WRITE_SP;
        cmd[1] = 0x00;
        if (op == OW_CONV_VDD) {
            cmd[2] = OW_P0_R0_AD | OW_P0_R0_IAD;
        } else {
            cmd[2] = OW_P0_R0_IAD;
        }
        if ((err = ow_SendCommands(dev, portnum, 3, &cmd[0])) < 0) {
            return (err);
        }

        // Send Copy Scratch Pad Page 0x00 command
        cmd[0] = OW_COPY_SP;
        cmd[1] = 0x00;
        if ((err = ow_SendCommands(dev, portnum, 2, &cmd[0])) < 0) {
            return (err);
        }

        // Loop until operation completed
        retry = 0;
        do {
            // read bit
            if ((err = ow_ReadBit(dev, portnum, &rd_bit)) < 0) {
                return (err);
            } else {
                retry++;
                if (retry > OW_MAX_RETRY_ON_CONVERT_BUSY) {
                    return (OWERROR_CONVERSION_TIMEOUT);
                }
            }
        } while (!rd_bit);

        // Send Convert Voltage command
        cmd[0] = OW_CONVERT_V;
        if ((err = ow_SendCommands(dev, portnum, 1, &cmd[0])) < 0) {
            return (err);
        }
    } else if (op == OW_CONV_I) {
        // nothing to do
    } else {
        return (OWERROR_ILLEGAL_CONVERSION);
    }

    // Loop until conversion completed
    retry = 0;
    do {
        // read bit
        if ((err = ow_ReadBit(dev, portnum, &rd_bit)) < 0) {
            return (err);
        } else {
            retry++;
            if (retry > OW_MAX_RETRY_ON_CONVERT_BUSY) {
                return (OWERROR_CONVERSION_TIMEOUT);
            }
        }
    } while (!rd_bit);

    // Send Recall Memory page 0x00 command
    cmd[0] = OW_RECALL_MEM;
    cmd[1] = 0x00;
    if ((err = ow_SendCommands(dev, portnum, 2, &cmd[0])) < 0) {
        return (err);
    }

    // Send Read Scratch Pad page 0x00 command
    cmd[0] = OW_READ_SP;
    cmd[1] = 0x00;
    if ((err = ow_SendCommands(dev, portnum, 2, &cmd[0])) < 0) {
        return (err);
    }

    // Send 9 Read Byte commands
    for (i = 0; i < 9; i++) {
        if ((err = ow_ReadByte(dev, portnum, &rd_bytes[i])) < 0) {
            return (err);
        }
    }

    // Check CRC
    if ((err = ow_CheckCRC(&rd_bytes[0], 8)) < 0) {
        return (err);
    };

    // Get result corresponding to desired conversion
    if (op == OW_CONV_TEMP) {
        *cv = (short) (((((short) rd_bytes[2]) << 8) | ((short) rd_bytes[1])) >> 3);
    } else if (op == OW_CONV_VDD) {
        *cv = (short) ((((short) rd_bytes[4]) << 8) | ((short) rd_bytes[3]));
    } else if (op == OW_CONV_VAD) {
        *cv = (short) ((((short) rd_bytes[4]) << 8) | ((short) rd_bytes[3]));
    } else if (op == OW_CONV_I) {
        *cv = (short) ((((short) rd_bytes[6]) << 8) | ((short) rd_bytes[5]));
    }

    return (0);
}
