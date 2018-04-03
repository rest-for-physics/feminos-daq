/*******************************************************************************
                           
                           _____________________

 File:        ksz9021.h

 Description: Various definitions for a Micrel KSZ9021 Ethernet PHY device.
 This PHY is used on Enclustra Mars MX2 module.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  May 2011: created 

*******************************************************************************/

#ifndef _KSZ9021
#define _KSZ9021

// Register 0
#define PHY_R0_SOFT_RESET       0x8000
#define PHY_R0_AUTONEG_ENABLE   0x1000
#define PHY_R0_RESTART_AUTONEG  0x0200

// Register 1
#define PHY_R1_AUTONEG_COMPLETE 0x0020

// Register 4 (auto negotiation adverstisement)
#define PHY_R4_ADV_IEEE802_3_CAP 0x0001
#define PHY_R4_ADV_10BT_HDX_CAP  0x0020
#define PHY_R4_ADV_10BT_FDX_CAP  0x0040
#define PHY_R4_ADV_100BT_HDX_CAP 0x0080
#define PHY_R4_ADV_100BT_FDX_CAP 0x0100

// Register 5 (link partner ability)
#define PHY_R5_NEXT_PAGE_CAP    0x8000
#define PHY_R5_PARTNER_WORD_ACK 0x4000
#define PHY_R5_REMOTE_FAULT     0x2000
#define PHY_R5_T4_CAPABLE       0x0200
#define PHY_R5_100_DUPLEX_CAP   0x0100
#define PHY_R5_100_HALF_CAP     0x0080
#define PHY_R5_10_DUPLEX_CAP    0x0040
#define PHY_R5_10_HALF_CAP      0x0020

// Register 9 (1000 BaseT Control)
#define PHY_R9_ADV_1000BT_HDX_CAP 0x0100
#define PHY_R9_ADV_1000BT_FDX_CAP 0x0200

// Register 31 (PHY Control)
#define PHY_R31_FINAL_IS_DUPLEX     0x0008
#define PHY_R31_FINAL_SPEED_IS_10   0x0010
#define PHY_R31_FINAL_SPEED_IS_100  0x0020
#define PHY_R31_FINAL_SPEED_IS_1000 0x0040

#endif

