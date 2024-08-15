/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        basic_ip.c

 Description: Basic functions for IP networking over Ethernet

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2012: created by extraction from sockudp_ps.h

  April 2013: extracted from rework of sockudp.* files

*******************************************************************************/
#include "ethernet.h"

/*******************************************************************************
 ip_checksum
 Simple function to calculate IP checksum.
 Likely to work only on a Big Endian processor
*******************************************************************************/
unsigned short ip_checksum(unsigned short* buf, unsigned short len) {
    unsigned int sum = 0;
    int i;
    unsigned char* c;

    // add up all 16-bit words of header
    for (i = 0; i < (len / 2); i++) {
        sum = sum + (unsigned int) *buf;
        buf++;
    }

    // add the last byte if the length is odd
    if (len & 0x1) {
        c = (unsigned char*) buf;
        sum = sum + (unsigned int) (*c);
    }

    // take only 16 bits out of the 32 bit sum and add up the carries
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // one's complement the result
    sum = ~sum;

    // printf("checksum=0x%x\n", (unsigned short) sum);

    return ((unsigned short) sum);
}
