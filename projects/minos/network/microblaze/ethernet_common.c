/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        ethernet_common.c

 Description: Implementation of simplified IP stack for MicroBlaze
 (Spartan 6) AXI Ethernet

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  2011: copied from T2K and adapted

  September 2012: added the functions SockAddr_Clear() and
  SockUDP_CopyLastRcvSockAddr(). Changed the prototype of SockUDP_Send().
  In the previous versions, the MAC/IP/ports of the sender was not made
  available to the user and sending one frame was implicitly done to the host
  that had sent the last datagram. The new functions allow saving the
  information on the sender upon receive and requires the destination
  MAC/IP/ports be supplied by the caller for send operations.

  October 2012: added argument fw to SockUDP_Send() in order to be able to
  put user supplied data in the first two payload bytes of a UDP datagram

  November 2012: simplified SockUDP_Send(). The new version supports only
  data supplied in one buffer and placed at the appropriate offset so that
  Ethernet, IP, and UDP headers can be added in front of user data in the
  same contiguous buffer.

  April 2013: file sockudp_common.c renamed to ethernet_common.c

*******************************************************************************/

#include "basic_ip.h"
#include "bufpool.h"
#include "ethernet.h"
#include "platform_spec.h"
#include <stdio.h>

#include "xbasic_types.h"
#include "xparameters.h"

/*******************************************************************************
 Ethernet_ProcessIP
 This function is called internally whenever an IP frame is received.
 It calls the appropriate handler function to process ICMP, UDP and TCP frames.
 Frames of other protocols are dropped silently
*******************************************************************************/
int Ethernet_ProcessIP(Ethernet* et, EthHdr* ehdr, unsigned int arg, int* from, void** data) {
    IPHdr* pip_hdr;

#ifdef USE_AXI_ETHERNET_DMA
    pip_hdr = (IPHdr*) ((unsigned int) ehdr + sizeof(EthHdr));
#endif // USE_AXI_ETHERNET_DMA

#ifdef USE_AXI_ETHERNET_FIFO
    IPHdr ip_hdr;
    UDPHdr udp_hdr;
    TCPHdr tcp_hdr;

    // Copy IP Header from LLTemac RX FIFO
    XLlFifo_Read(&(et->FifoInstance), (void*) &ip_hdr, (Xuint32) (sizeof(IPHdr)));
    pip_hdr = &ip_hdr;
#endif // USE_AXI_ETHERNET_FIFO

    // Check destination IP address
    if (!((pip_hdr->dstip[0] == et->loc_ip[0]) &&
          (pip_hdr->dstip[1] == et->loc_ip[1]) &&
          (pip_hdr->dstip[2] == et->loc_ip[2]) &&
          (pip_hdr->dstip[3] == et->loc_ip[3]))) {
        et->rx_ip_drop++;
        return (0);
    }

    // Check IP checksum
    if (ip_checksum((unsigned short*) pip_hdr, sizeof(IPHdr))) {
        et->rx_ip_drop++;
        return (0);
    }

    // IP datagram is OK
    et->rx_ip_cnt++;

    /*
    printf("ip.verlen = 0x%x\n", pip_hdr->verlen);
    printf("ip.tos    = 0x%x\n", pip_hdr->tos);
    printf("ip.len    = %d\n", ntohs(pip_hdr->len));
    printf("ip.id     = 0x%x\n", ntohs(pip_hdr->id));
    printf("ip.frag   = 0x%x\n", ntohs(pip_hdr->frag));
    printf("ip.ttl    = 0x%x\n", pip_hdr->ttl);
    printf("ip.proto  = 0x%x\n", pip_hdr->proto);
    printf("ip.check  = 0x%x\n", ntohs(pip_hdr->chk));
    printf("ip.srcip  = %d.%d.%d.%d\n", pip_hdr->srcip[0], pip_hdr->srcip[1], pip_hdr->srcip[2], pip_hdr->srcip[3]);
    */

    // Check for UDP datagram
    if (pip_hdr->proto == IP_PROTO_UDP) {
        return (Ethernet_ProcessUDP(et, ehdr, pip_hdr, arg, from, data));
    }

    // Check for TCP
    else if (pip_hdr->proto == IP_PROTO_TCP) {
        return (Ethernet_ProcessTCP(et, ehdr, pip_hdr, arg, from, data));
    }

    // Check ICMP message
    else if (pip_hdr->proto == IP_PROTO_ICMP) {
        return (Ethernet_ProcessICMP(et, ehdr, pip_hdr, arg));
    }

    // Other types of protocol are ignored
    else {
        et->rx_ip_drop++;
        return (0);
    }
    return (0);
}

/*******************************************************************************
 Ethernet_Close
 This function is called by the application release the resources of the
 interface.
*******************************************************************************/
int Ethernet_Close(Ethernet* et) {
    return (0);
}

/*******************************************************************************
 Ethernet_StatShow
 This function is dumps interface statistics on stdout
*******************************************************************************/
void Ethernet_StatShow(Ethernet* et) {
    int i;

    printf("MAC Address  : %x", et->loc_mac[0]);
    for (i = 1; i < 6; i++) {
        printf(":%x", et->loc_mac[i]);
    }
    printf("\n");
    printf("IP Address   : %d", et->loc_ip[0]);
    for (i = 1; i < 4; i++) {
        printf(".%d", et->loc_ip[i]);
    }
    printf("\n");
    printf("Ether Rx      : %d\n", et->rx_eth_fr_cnt);
    printf("Ether Rx drop : %d\n", et->rx_eth_drop);
    printf("Ether Tx      : %d\n", et->tx_eth_fr_cnt);
    printf("ARP Requests  : %d\n", et->rx_arp_req_cnt);
    printf("ARP Replies   : %d\n", et->tx_arp_rep_cnt);
    printf("IP Rx         : %d\n", et->rx_ip_cnt);
    printf("IP Tx         : %d\n", et->tx_ip_cnt);
    printf("IP Rx drop    : %d\n", et->rx_ip_drop);
    printf("ICMP Rx       : %d\n", et->rx_icmp);
    printf("ICMP Rx drop  : %d\n", et->rx_icmp_drop);
    printf("ICMP Tx       : %d\n", et->tx_icmp);
    printf("UDP Rx        : %d\n", et->rx_udp);
    printf("UDP Rx drop   : %d\n", et->rx_udp_drop);
    printf("UDP Tx        : %d\n", et->tx_udp);
    printf("TCP Rx        : %d\n", et->rx_tcp);
    printf("TCP Rx drop   : %d\n", et->rx_tcp_drop);
    printf("TCP Tx        : %d\n", et->tx_tcp);
    printf("\n");
}

/*******************************************************************************
 Ethernet_StatClear
 This function clears Ethernet statistics
*******************************************************************************/
void Ethernet_StatClear(Ethernet* et) {
    et->rx_eth_fr_cnt = 0;
    et->rx_eth_drop = 0;
    et->tx_eth_fr_cnt = 0;
    et->rx_arp_req_cnt = 0;
    et->tx_arp_rep_cnt = 0;
    et->rx_ip_cnt = 0;
    et->tx_ip_cnt = 0;
    et->rx_ip_drop = 0;
    et->rx_icmp = 0;
    et->tx_icmp = 0;
    et->rx_icmp_drop = 0;
    et->rx_udp = 0;
    et->rx_udp_drop = 0;
    et->tx_udp = 0;
    et->rx_tcp = 0;
    et->rx_tcp_drop = 0;
    et->tx_tcp = 0;
}
