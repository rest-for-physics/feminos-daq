/*******************************************************************************

                           _____________________

 File:        sockudp_axififo.c

 Description: Implementation of a minimal UDP/IP socket interface for Xilinx
 AXI Ethernet controller + LL FIFOs.

 At first SockUDP_Open(...) is called with parameters to specify the MAC
 address and IP address of this interface.
 The application has to call SockUDP_Recv(...) in polling mode.
 Internally, this function does the following:
 - it responds to any ARP request for this interface,
 - it responds to ICMP echo requests,
 - when a UDP frame is received, it saves internally the source MAC and IP
 addresses as well as the UDP source and destination ports, and copies out UDP
 data for the user
 - all other types of frames are dropped silently.
 When the application has processed the UDP data, it should call
 SockUDP_Send(...). This function does the following:
 - it fills the Ethernet, IP and UDP headers with the information saved from
 the last UDP frame received,
 - it appends the data supplied by the caller,
 - it sends the frame.
 Ressources are released by calling SockUDP_Close(...)

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  May 2011: created from T2K/PPC405 project files

*******************************************************************************/

#include <stdio.h>
#include "platform_spec.h"
#include "sockudp.h"
#include "xparameters.h"
#include "ksz9021.h"

#define ETHLITE_MAX_RETRY 1000

/*******************************************************************************
 SockUDP_Open
 Program the MAC and IP address of this interface and enable frame reception 
*******************************************************************************/
int SockUDP_Open(SockUDP *su, SockUDPParam *sup)
{
	XAxiEthernet_Config cfg;
	int status;
	int i;
	char mac_rb[8];
	unsigned int ByteCount;
	unsigned int TxVacancy;
	unsigned short speed;
	int isduplex;
	int isup;
	unsigned short PhyData;
	volatile unsigned int waste;

	// Initialize LL FIFO
	XLlFifo_Initialize(&(su->FifoInstance), XPAR_ETHERNET_FIFO_BASEADDR);

	// Disable XLLFifo interrupts
	XLlFifo_IntDisable(&(su->FifoInstance), XLLF_INT_ALL_MASK);

	// Clear all pending interrupts
	XLlFifo_IntClear(&(su->FifoInstance), XLLF_INT_ALL_MASK);

	// Read XLLFifo Rx occupancy
	ByteCount = XLlFifo_RxOccupancy(&(su->FifoInstance));
	xil_printf("XLlFifo_RxOccupancy: %d bytes\r\n", (ByteCount*4));

	// Read XLLFifo Tx vacancy
	TxVacancy = XLlFifo_TxVacancy(&(su->FifoInstance));
	xil_printf("XLlFifo_TxVacancy: %d bytes\r\n", (TxVacancy*4));

	cfg.Avb               = XPAR_AXIETHERNET_0_AVB;
	cfg.AxiDevBaseAddress = XPAR_AXIETHERNET_0_CONNECTED_BASEADDR;
	cfg.AxiDevType        = XPAR_AXIETHERNET_0_TEMAC_TYPE;
	cfg.AxiDmaRxIntr      = 0;
	cfg.AxiDmaTxIntr      = 0;
	cfg.AxiFifoIntr       = XPAR_AXIETHERNET_0_CONNECTED_FIFO_INTR;
	cfg.BaseAddress       = XPAR_AXIETHERNET_0_BASEADDR;
	cfg.DeviceId          = XPAR_AXIETHERNET_0_DEVICE_ID;
	cfg.ExtMcast          = XPAR_AXIETHERNET_0_MCAST_EXTEND;
	cfg.PhyType           = XPAR_AXIETHERNET_0_PHY_TYPE;
	cfg.RxCsum            = XPAR_AXIETHERNET_0_RXCSUM;
	cfg.RxVlanStrp        = XPAR_AXIETHERNET_0_RXVLAN_STRP;
	cfg.RxVlanTag         = XPAR_AXIETHERNET_0_RXVLAN_TAG;
	cfg.RxVlanTran        = XPAR_AXIETHERNET_0_RXVLAN_TRAN;
	cfg.Stats             = XPAR_AXIETHERNET_0_STATS;
	cfg.TemacIntr         = XPAR_AXIETHERNET_0_INTR;
	cfg.TemacType         = XPAR_AXIETHERNET_0_TEMAC_TYPE;
	cfg.TxCsum            = XPAR_AXIETHERNET_0_TXCSUM;
	cfg.TxVlanStrp        = XPAR_AXIETHERNET_0_TXVLAN_STRP;
	cfg.TxVlanTag         = XPAR_AXIETHERNET_0_TXVLAN_TAG;
	cfg.TxVlanTran        = XPAR_AXIETHERNET_0_RXVLAN_TRAN;

	// Initialize Axi Ethernet
	if ((status = XAxiEthernet_CfgInitialize(&(su->xaxie), &cfg, XPAR_AXIETHERNET_0_BASEADDR)) != XST_SUCCESS)
	{
		xil_printf("XAxiEthernet_CfgInitialize failed.\r\n");
		return (-1);
	}

	// Set MAC address
	if ((status = XAxiEthernet_SetMacAddress (&(su->xaxie), &sup->loc_mac[0])) != XST_SUCCESS)
	{
		xil_printf("XAxiEthernet_SetMacAddress failed.\r\n");
		return (-1);
	}

	// Read-back MAC address
	XAxiEthernet_GetMacAddress (&(su->xaxie), (void*) &(mac_rb[0]));
	xil_printf("MAC = %02x.%02x.%02x.%02x.%02x.%02x\r\n",
			mac_rb[0], mac_rb[1], mac_rb[2], mac_rb[3], mac_rb[4], mac_rb[5]);

	// Save MAC address 
	for (i=0;i<6;i++)
	{
		su->loc_mac[i] = sup->loc_mac[i];
	}

	// Save local IP address
	for (i=0;i<4;i++)
	{
		su->loc_ip[i] = sup->loc_ip[i];
	}

	// Save buffer manager address
	su->bp = sup->bp;
	
	// Clear Statistics
	su->rx_eth_fr_cnt  = 0;
	su->tx_eth_fr_cnt  = 0;
	su->rx_arp_req_cnt = 0;
	su->tx_arp_rep_cnt = 0;
	su->rx_ip_cnt      = 0;
	su->rx_ip_err      = 0;
	su->rx_udp         = 0;
	su->rx_icmp        = 0;
	su->tx_ip_cnt      = 0;
	su->tx_icmp        = 0;
	su->tx_udp         = 0;

	// Clear remote MAC address
	for (i=0;i<6;i++)
	{
		su->rem_mac[i] = 0x00;
	}

	// Clear remote IP address
	for (i=0;i<4;i++)
	{
		su->rem_ip[i] = 0x00;
	}

	// Set operating speed
	if ((status = XAxiEthernet_SetOperatingSpeed(&(su->xaxie), 1000)) != XST_SUCCESS)
	{
		xil_printf("XAxiEthernet_SetOperatingSpeed failed.\r\n");
		return (-1);
	}

	// Set MDIO divisor to access PHY registers
	// Must be less than 2.5 MHz. We have: 100 MHz / ((31 + 1) * 2) = 1.56 MHz
	XAxiEthernet_PhySetMdioDivisor(&(su->xaxie), 31);

	// Enable RX and TX clock skew to work with RGMII1.3 interface
	XAxiEthernet_PhyWrite(&(su->xaxie), 1, 0xC, 0xF7F7);
	XAxiEthernet_PhyWrite(&(su->xaxie), 1, 0xB, 0x8104);

	// Reset PHY and make auto-negociation
	XAxiEthernet_PhyWrite(&(su->xaxie), 1, 0, (PHY_R0_SOFT_RESET | PHY_R0_AUTONEG_ENABLE));
	XAxiEthernet_PhyWrite(&(su->xaxie), 1, 0, (PHY_R0_RESTART_AUTONEG | PHY_R0_AUTONEG_ENABLE));

	// Wait until auto-negotiation done
	PhyData = 0;
	i = 0;
	do
	{
		for (waste=0; waste<1000000; waste++) ;
		XAxiEthernet_PhyRead(&(su->xaxie), 1, 1, &PhyData);
		i++;
		if (i >= 50)
		{
			xil_printf("SockUDP_Open: Ethernet speed auto-negotiation timed-out.\r\n");
			return(-1);
		}
	} while (!(PhyData & PHY_R1_AUTONEG_COMPLETE));
	xil_printf("Auto-negotiation completed after %d loops.\r\n", i);

	// Print Basic Control Register (register 0)
	XAxiEthernet_PhyRead(&(su->xaxie), 1, 0, &PhyData);
	xil_printf("Phy Register(%d) = 0x%x (Basic Control Register)\r\n", 0, PhyData);

	// Print Basic Status Register (register 1)
	XAxiEthernet_PhyRead(&(su->xaxie), 1, 1, &PhyData);
	xil_printf("Phy Register(%d) = 0x%x (Basic Status Register)\r\n", 1, PhyData);

	// Print Auto-negociation advertisement (register 4)
	XAxiEthernet_PhyRead(&(su->xaxie), 1, 4, &PhyData);
	xil_printf("Phy Register(%d) = 0x%x (Auto-negociation advertisement)\r\n", 4, PhyData);

	// Print Link Partner Ability (register 5)
	XAxiEthernet_PhyRead(&(su->xaxie), 1, 5, &PhyData);
	xil_printf("Phy Register(%d) = 0x%x (Link Partner Ability)\r\n", 5, PhyData);

	// Print Extended MII Status (register 15)
	XAxiEthernet_PhyRead(&(su->xaxie), 1, 15, &PhyData);
	xil_printf("Phy Register(%d) = 0x%x (Extended MII Status)\r\n", 15, PhyData);

	// Print Digital PMA/PCS status (register 19)
	XAxiEthernet_PhyRead(&(su->xaxie), 1, 19, &PhyData);
	xil_printf("Phy Register(%d) = 0x%x (Digital PMA/PCS status)\r\n", 19, PhyData);

	// Print RXER Counter (register 21)
	XAxiEthernet_PhyRead(&(su->xaxie), 1, 21, &PhyData);
	xil_printf("Phy Register(%d) = 0x%x (RXER Counter)\r\n", 21, PhyData);

	// Print PHY Control (register 31)
	XAxiEthernet_PhyRead(&(su->xaxie), 1, 31, &PhyData);
	xil_printf("Phy Register(%d) = 0x%x (PHY Control)\r\n", 31, PhyData);

	// Get operating speed
	speed = XAxiEthernet_GetOperatingSpeed(&(su->xaxie));
	xil_printf("XAxiEthernet_GetOperatingSpeed: %d\r\n", speed);

	// Set Ethernet options
	if ((status = XAxiEthernet_SetOptions(&(su->xaxie), XAE_TRANSMITTER_ENABLE_OPTION | XAE_RECEIVER_ENABLE_OPTION | XAE_FCS_INSERT_OPTION)) != XST_SUCCESS)
	{
		xil_printf("XAxiEthernet_SetOptions failed.\r\n");
		return (-1);
	}

	// Start Ethernet
	XAxiEthernet_Start(&(su->xaxie));

	// Check RGMII status
	// The documentation says this register is only available in the hard Temac,
	// so we should not try to read it
	/*
	if ((status = XAxiEthernet_GetRgmiiStatus (&(su->xaxie), &speed, &isduplex, &isup)) != XST_SUCCESS)
	{
		xil_printf("XAxiEthernet_GetRgmiiStatus failed.\r\n");
		return (-1);
	}
	xil_printf (" speed=%d isduplex=%d isup=%d\r\n ", speed, isduplex, isup);
	*/

	return(0);
}

/*******************************************************************************
 SockUDP_SendFrame
 Send an Ethernet frame composed of several buffers
 This function copies the (possibly scattered) supplied frame to the
 consecutive, word aligned transmit buffer of the Ethernet controller and
 sends the frame.
 Return : 0 on success
         -1 on failure  
*******************************************************************************/
int SockUDP_SendFrame(SockUDP *su, SGListItem *sgl, int num_el)
{	
	int i;
	int err;
	short sz;
	Xuint32 TxVacancy;

	// Check if the previous frame was sent
	if (XLlFifo_IsTxDone(&(su->FifoInstance)))
	{
		// Clear pending Transmit Complete interrupt
		XLlFifo_IntClear(&(su->FifoInstance), XLLF_INT_TC_MASK);

		su->tx_eth_fr_cnt++;
		xil_printf("SockUDP_SendFrame: Sent Frame Count: %d\r\n", su->tx_eth_fr_cnt);
	}

	// Copy frame to send
	sz = 0;
	for (i=0;i<num_el;i++)
	{
		if (i == (num_el-1))
		{
			// Force minimum frame length to be reached
			if ( (sz+(sgl+i)->len) < MAC_MINIMUM_FRAME_SIZE)
			{
				(sgl+i)->len = MAC_MINIMUM_FRAME_SIZE - sz;
			}
		}

		// Check that maximum frame size will not be reached
		if ((sz+(sgl+i)->len) >= MAC_MAXIMUM_FRAME_SIZE)
		{
			return(-1);
		}

		// Check that the TX Fifo Vacancy is sufficient
		TxVacancy = 4 * XLlFifo_TxVacancy(&(su->FifoInstance));
		if ((sgl+i)->len > TxVacancy)
		{
			return (-1);
		}

		// Copy data to the TX FIFO
		XLlFifo_Write(&(su->FifoInstance), (sgl+i)->buf, (Xuint32) ((sgl+i)->len));

		// Return Tx buffer to Buffer Pool
		if ((err = BufPool_ReturnBuffer(su->bp, (void *) ((sgl+i)->buf))) < 0)
		{
			return(err);
		}

		sz += (sgl+i)->len;
	}

	// Send frame
	XLlFifo_TxSetLen(&(su->FifoInstance), (Xuint32) sz);
	xil_printf("SockUDP_SendFrame done\r\n");

	return(0);
}

/*******************************************************************************
 SockUDP_Recv
 This function is called by the application. It handles internally the frames
 other than UDP that should be processed and returns only to the user the
 content of the UDP datagrams received. 
*******************************************************************************/
int SockUDP_Recv(SockUDP *su, void* *data, short *sz)
{
	int tl = 0;
	EthHdr eth;
	int ByteCount;
	unsigned int RxOccupancy;
	
	// Check if a new frame has arrived
	if ((RxOccupancy = XLlFifo_RxOccupancy(&(su->FifoInstance))) == 0)
	{
		//xil_printf ("frame not received\r\n");
		* sz = 0;
		return (0);
	}

	// Get the size of the received frame
	if ((ByteCount = XLlFifo_RxGetLen(&(su->FifoInstance))) == 0)
	{
		*sz = 0;
		return(0);
	}
	//xil_printf("XLlFifo_RxGetLen: frame received size=%d\r\n", ByteCount);

	// Check that the RX FIFO occupancy is at least the size of the received frame
	if ((RxOccupancy*4) < (ByteCount+4))
	{
		return (0);
	}
	xil_printf ("Frame received!\r\n");
	su->rx_eth_fr_cnt++;

	// Read Ethernet frame header
	XLlFifo_Read(&(su->FifoInstance), (void *) &eth, (sizeof(EthHdr)));
	xil_printf (" eth.mac_to: %x:%x:%x:%x:%x:%x\r\n", eth.mac_to[0], eth.mac_to[1], eth.mac_to[2], eth.mac_to[3], eth.mac_to[4], eth.mac_to[5]);
	xil_printf (" su->loc_mac: %x:%x:%x:%x:%x:%x\r\n", su->loc_mac[0], su->loc_mac[1], su->loc_mac[2], su->loc_mac[3], su->loc_mac[4], su->loc_mac[5]);
	xil_printf (" eth.tl: %x \r\n ", ntohs(eth.tl));
	xil_printf (" ETH_TYPE_LEN_ARP: %x \r\n ",ETH_TYPE_LEN_ARP);
	// Check for ARP request both on destination broadcast and local MAC address
	if (
		((eth.mac_to[0] == 0xFF) &&
		(eth.mac_to[1] == 0xFF) &&
		(eth.mac_to[2] == 0xFF) &&
		(eth.mac_to[3] == 0xFF) &&
		(eth.mac_to[4] == 0xFF) &&
		(eth.mac_to[5] == 0xFF) &&
		(ntohs(eth.tl) == ETH_TYPE_LEN_ARP))
		||
		((eth.mac_to[0] == su->loc_mac[0]) &&
		(eth.mac_to[1] == su->loc_mac[1]) &&
		(eth.mac_to[2] == su->loc_mac[2]) &&
		(eth.mac_to[3] == su->loc_mac[3]) &&
		(eth.mac_to[4] == su->loc_mac[4]) &&
		(eth.mac_to[5] == su->loc_mac[5]) &&
		(ntohs(eth.tl) == ETH_TYPE_LEN_ARP))
		)
	{ 
		tl = SockUDP_ProcessARP(su, ByteCount);
	
		// the frame is not returned to the upper layer, but any error is forwarded
		* sz = 0;
		if (tl >= 0)
		{
			
			return(0);
		}
		else
		{
			//xil_printf (" tl= %d\r\n",tl);
			return (tl);
		}	
	}

	// Process unicast IP frames
	else if ((eth.mac_to[0] == su->loc_mac[0]) &&
		(eth.mac_to[1] == su->loc_mac[1]) &&
		(eth.mac_to[2] == su->loc_mac[2]) &&
		(eth.mac_to[3] == su->loc_mac[3]) &&
		(eth.mac_to[4] == su->loc_mac[4]) &&
		(eth.mac_to[5] == su->loc_mac[5]) &&
		(ntohs(eth.tl)  == ETH_TYPE_LEN_IP))
	{
		//xil_printf (" eth.mac_to: %x:%x:%x:%x:%x:%x\r\n", eth.mac_to[0], eth.mac_to[1], eth.mac_to[2], eth.mac_to[3], eth.mac_to[4], eth.mac_to[5]);  
		//xil_printf (" su->loc_mac: %x:%x:%x:%x:%x:%x\r\n", su->loc_mac[0], su->loc_mac[1], su->loc_mac[2], su->loc_mac[3], su->loc_mac[4], su->loc_mac[5]);  
		//xil_printf (" eth.tl: %x \r\n ",eth.tl);
		tl = SockUDP_ProcessIP(su, &eth, ByteCount, data);
	}

	// Drop all other frames
	else
	{
		//xil_printf (" frame dropped !/r/n");  
	
		tl = 0;
		*sz = 0;
	}

	// Enable frame reception
	//xil_printf ("  Enable frame reception !/r/n");
		
	if (tl >= 0)
	{
		*sz = (short) tl;
	}
	else
	{
		*sz = 0;
	}
	//xil_printf (" received frame end/r/n");  
	return(tl);
}

