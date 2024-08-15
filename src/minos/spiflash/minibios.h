/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        minibios.h

 Description: Mini BIOS for Feminos card on Enclustra Mars MX2 module

 Definition of a minimal "bios" to store/retrieve non-volatile configuation
 parameters for the Feminos card.
 These parameters are stored in the last page (256 bytes) of the last sector
 (4 KB) of the SPI Flash of Mars MX2 module

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2011: created from T2K version

  April 2012: added external function Minibios_IsBiosButtonPressed() to be
  able to re-use the same code for Feminos and TCM

  October 2012: added PLL field for TCM

  Janaury 2014: added field pll_out_delay (used on TCM with PLL)

*******************************************************************************/

#ifndef MINIBIOS_H
#define MINIBIOS_H

typedef struct _Minibios_Data {
    unsigned short mtu;     // MTU size
    unsigned short speed;   // desired speed
    unsigned char mac[6];   // Ethernet MAC address of reduced DCC
    unsigned char ip[4];    // IP address of reduced DCC
    unsigned char card_id;  // ID of this Feminos card in [0;31]
    unsigned char pll;      // PLL present/not present (TCM only)
    unsigned char pll_odel; // PLL output clock fine delay (TCM with PLL only)
    unsigned char fecon;    // Power ON FEC at startup (Feminos only)
} Minibios_Data;

extern Minibios_Data minibiosdata;

int Minibios(Minibios_Data* ma);
int Minibios_Command(Minibios_Data* ma, char* cmd, char* rep);
int Minibios_IsBiosButtonPressed();

#endif
