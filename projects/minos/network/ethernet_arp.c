/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        ethernet_arp.c

 Description: Ethernet ARP frame processing
 MicroBlaze (Spartan 6) AXI Ethernet

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  November 2012: extracted from sockudp_common.c

*******************************************************************************/

#include <stdio.h>
#include "platform_spec.h"
#include "ethernet.h"
#include "bufpool.h"

#include "xparameters.h"
#include "xbasic_types.h"

/*******************************************************************************
 Ethernet_ProcessARP
 This function is called internally whenever an ARP frame is received.
 If the ARP is a request for this interface, it makes a response and calls
 the function to send the ARP response frame.  
*******************************************************************************/
int Ethernet_ProcessARP(Ethernet *et, unsigned int arg)
{
	int i;
	int err;
#ifdef USE_AXI_ETHERNET_FIFO
	Arp_ReqRep arp_req;
#endif
	unsigned char *tx_fr;
	EthHdr     *eth_rep;
	Arp_ReqRep *arp_rep;
	Arp_ReqRep *parp_req;
	SGListItem sgl[1];

#ifdef USE_AXI_ETHERNET_DMA
	parp_req = (Arp_ReqRep *) (XAxiDma_BdGetBufAddr(et->CurRxBdPtr) + sizeof(EthHdr));
#endif

#ifdef USE_AXI_ETHERNET_FIFO
	// Copy ARP frame from LLTemac RX Fifo
	XLlFifo_Read(&(et->FifoInstance), (void *)&arp_req, (Xuint32) (arg - sizeof(EthHdr)));
	parp_req = &arp_req;
#endif // USE_AXI_ETHERNET_FIFO
	
	/*
	printf("Ethernet_ProcessARP: ar_op=0x%x ar_hrd=0x%x ar_pro=0x%x ar_hln=0x%x ar_pln=0x%x.\n",
		ntohs(parp_req->ar_op),
		ntohs(parp_req->ar_hrd),
		ntohs(parp_req->ar_pro),
		parp_req->ar_hln,
		parp_req->ar_pln);

	printf("Ethernet_ProcessARP: target_ip=%d.%d.%d.%d.\n",
		parp_req->ar_tip[0],
		parp_req->ar_tip[1],
		parp_req->ar_tip[2],
		parp_req->ar_tip[3]);
		*/

	// Check if it is an ARP request for this interface
	if (! (
		(ntohs(parp_req->ar_op) == ARP_REQUEST) &&
		(ntohs(parp_req->ar_hrd) == 0x0001) &&
		(ntohs(parp_req->ar_pro) == 0x0800) &&
		(parp_req->ar_hln == 0x06) &&
		(parp_req->ar_pln == 0x04) &&
		(parp_req->ar_tip[0] == et->loc_ip[0]) &&
		(parp_req->ar_tip[1] == et->loc_ip[1]) &&
		(parp_req->ar_tip[2] == et->loc_ip[2]) &&
		(parp_req->ar_tip[3] == et->loc_ip[3])
		))
	{
		return (0);
	}
	//printf("Ethernet_ProcessARP: received ARP for this interface.\n");
	
	// This is an ARP request for this interface, so send an ARP reply
	
#ifdef USE_AXI_ETHERNET_DMA
	if ((err = BufPool_GiveBuffer(et->bp, (void *) &tx_fr, AUTO_RETURNED)) < 0)
	{
		return (err);
	}
#endif // USE_AXI_ETHERNET_DMA

#ifdef USE_AXI_ETHERNET_FIFO
 	if ((err = BufPool_GiveBuffer(et->bp, (void *) &tx_fr, AUTO_RETURNED)) < 0)
	{
		return (err);
	}
#endif // USE_AXI_ETHERNET_FIFO

	et->rx_arp_req_cnt++;
	eth_rep = (EthHdr *) tx_fr;
	arp_rep = (Arp_ReqRep *) (tx_fr + sizeof(EthHdr));
		
	// Prepare source/target MAC addresses
	for (i=0;i<6;i++)
	{
		eth_rep->mac_to[i]   = parp_req->ar_sha[i];
		eth_rep->mac_from[i] = et->loc_mac[i];

		arp_rep->ar_tha[i]   = parp_req->ar_sha[i];
		arp_rep->ar_sha[i]   = et->loc_mac[i];
	}
	
	// Prepare source/target IP addresses
	for (i=0;i<4;i++)
	{
		arp_rep->ar_sip[i] = et->loc_ip[i];
		arp_rep->ar_tip[i] = parp_req->ar_sip[i];
	}
	
	// Fill other fields of ARP reply
	eth_rep->tl      = htons(ETH_TYPE_LEN_ARP);
	arp_rep->ar_hrd  = htons(0x0001);
	arp_rep->ar_pro  = htons(0x0800);
	arp_rep->ar_hln  = 6;
	arp_rep->ar_pln  = 4;
	arp_rep->ar_op   = htons(ARP_REPLY);

	// Make Scatter-gather list with single item to send
	sgl[0].buf   = (void *) tx_fr;
	sgl[0].len   = SIZEOF_ARP_REPLY_FRAME;
	sgl[0].flags = RETURN_BUFFER_WHEN_SENT;
	et->tx_arp_rep_cnt++;
	return (Ethernet_SendFrame(et, &sgl[0], 1));
}
