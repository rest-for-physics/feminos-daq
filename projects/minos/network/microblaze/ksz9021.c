/*******************************************************************************

                           _____________________

 File:        ksz9021.c

 Description: Code specific to Micrel KSZ9021 Ethernet PHY.
 This device is used on Enclustra Mars MX2 Spartan-6 module.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  May 2011: created

*******************************************************************************/

#include <stdio.h>
#include "ksz9021.h"
#include "ethernet.h"

/*******************************************************************************
 Ethernet_InitPhy
*******************************************************************************/
int Ethernet_InitPhy(Ethernet *et)
{
	int i;
	volatile int waste;
	unsigned short PhyData;

	// Set MDIO divisor to access PHY registers
	// Must be less than 2.5 MHz. We have: 100 MHz / ((31 + 1) * 2) = 1.56 MHz
	XAxiEthernet_PhySetMdioDivisor(&(et->xaxie), 31);

	// Enable RX and TX clock skew to work with RGMII1.3 interface
	XAxiEthernet_PhyWrite(&(et->xaxie), 1, 0xC, 0xF7F7);
	XAxiEthernet_PhyWrite(&(et->xaxie), 1, 0xB, 0x8104);

	// Reset PHY
	XAxiEthernet_PhyWrite(&(et->xaxie), 1, 0, (PHY_R0_SOFT_RESET | PHY_R0_AUTONEG_ENABLE));

	// Set advertised capability according to desired speed
	if (et->des_speed == 1000)
	{
		// Nothing to do
	}
	if (et->des_speed <= 100)
	{
		// Disable 1000 capability
		XAxiEthernet_PhyWrite(&(et->xaxie), 1, 0x9, 0);
	}
	if (et->des_speed <= 10)
	{
		// Disable 100 capability, i.e. advertise only 10 Mbps capability
		XAxiEthernet_PhyWrite(&(et->xaxie), 1, 0x4, PHY_R4_ADV_IEEE802_3_CAP | PHY_R4_ADV_10BT_HDX_CAP | PHY_R4_ADV_10BT_FDX_CAP);
	}

	// Make auto-negociation
	XAxiEthernet_PhyWrite(&(et->xaxie), 1, 0, (PHY_R0_RESTART_AUTONEG | PHY_R0_AUTONEG_ENABLE));

	// Wait until auto-negotiation done
	PhyData = 0;
	i = 0;
	do
	{
		for (waste=0; waste<1000000; waste++) ;
		XAxiEthernet_PhyRead(&(et->xaxie), 1, 1, &PhyData);
		i++;
		if (i >= 50)
		{
			printf("Ethernet_InitPhy: Ethernet speed auto-negotiation timed-out.\r\n");
			return(-1);
		}
	} while (!(PhyData & PHY_R1_AUTONEG_COMPLETE));
	//printf("Auto-negotiation completed after %d loops.\n", i);

	// Print Basic Control Register (register 0)
	//XAxiEthernet_PhyRead(&(su->xaxie), 1, 0, &PhyData);
	//printf("Phy Register(%d) = 0x%x (Basic Control Register)\n", 0, PhyData);

	// Print Basic Status Register (register 1)
	//XAxiEthernet_PhyRead(&(su->xaxie), 1, 1, &PhyData);
	//printf("Phy Register(%d) = 0x%x (Basic Status Register)\n", 1, PhyData);

	// Print Auto-negociation advertisement (register 4)
	//XAxiEthernet_PhyRead(&(su->xaxie), 1, 4, &PhyData);
	//printf("Phy Register(%d) = 0x%x (Auto-negociation advertisement)\n", 4, PhyData);

	// Print Link Partner Ability (register 5)
	//XAxiEthernet_PhyRead(&(su->xaxie), 1, 5, &PhyData);
	//printf("Phy Register(%d) = 0x%x (Link Partner Ability)\n", 5, PhyData);

	// Print 1000 BaseT control (register 9)
	//XAxiEthernet_PhyRead(&(su->xaxie), 1, 9, &PhyData);
	//printf("Phy Register(%d) = 0x%x (1000 BaseT control)\n", 9, PhyData);

	// Print Extended MII Status (register 15)
	//XAxiEthernet_PhyRead(&(su->xaxie), 1, 15, &PhyData);
	//printf("Phy Register(%d) = 0x%x (Extended MII Status)\n", 15, PhyData);

	// Print Digital PMA/PCS status (register 19)
	//XAxiEthernet_PhyRead(&(su->xaxie), 1, 19, &PhyData);
	//printf("Phy Register(%d) = 0x%x (Digital PMA/PCS status)\n", 19, PhyData);

	// Print RXER Counter (register 21)
	//XAxiEthernet_PhyRead(&(su->xaxie), 1, 21, &PhyData);
	//printf("Phy Register(%d) = 0x%x (RXER Counter)\n", 21, PhyData);

	// Print PHY Control (register 31)
	XAxiEthernet_PhyRead(&(et->xaxie), 1, 31, &PhyData);
	//printf("Phy Register(%d) = 0x%x (PHY Control)\n", 31, PhyData);

	// Determine the final speed that was negociated
	if (PhyData & PHY_R31_FINAL_SPEED_IS_10)
	{
		et->actual_speed = 10;
	}
	else if (PhyData & PHY_R31_FINAL_SPEED_IS_100)
	{
		et->actual_speed = 100;
	}
	else if (PhyData & PHY_R31_FINAL_SPEED_IS_1000)
	{
		et->actual_speed = 1000;
	}
	else
	{
		et->actual_speed = 0;
		printf("Ethernet_InitPhy: could not determine negotiated speed (Phy Register(31) = 0x%x)\r\n", PhyData);
		return (-1);
	}

	// See if the connection is full duplex or half
	if (PhyData & PHY_R31_FINAL_IS_DUPLEX)
	{
		et->isduplex = 1;
	}
	else
	{
		et->isduplex = 0;
	}

	// Show final negotiation result
	printf("Ethernet_InitPhy: negotiated speed: %d Mbps ", et->actual_speed);
	if (et->isduplex == 1)
	{
		printf("Full-duplex\n");
	}
	else
	{
		printf("Half-duplex\n");
	}

	return(0);
}

