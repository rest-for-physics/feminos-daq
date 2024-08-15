/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        ethernet_icmp.c

 Description: ICMP processing for MicroBlaze (Spartan 6) AXI Ethernet

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  November 2012: extracted from sockudp_common.c

*******************************************************************************/

#include "bufpool.h"
#include "ethernet.h"
#include "platform_spec.h"
#include <stdio.h>

#include "xbasic_types.h"
#include "xparameters.h"

/*******************************************************************************
 Ethernet_ProcessICMP
 This function is called internally whenever an ICMP frame is received.
 If it is an ICMP request for this interface, a response is prepared  and a call
 is made to send the frame.
*******************************************************************************/
int Ethernet_ProcessICMP(Ethernet* et, EthHdr* ehdr, IPHdr* iphdr, unsigned int arg) {
    ICMPHdr* icmp_hdr;
#ifndef USE_AXI_ETHERNET_DMA
    ICMPMsg icmp_msg;
#endif
    unsigned char* icmp_data_src;
    EthHdr* eth_rep;
    IPHdr* ip_rep;
    ICMPHdr* icmp_rep;
    int i;
    int err;
    unsigned char* data_icmp;
    int icmp_data_len;
    SGListItem sgl[1];

#ifdef USE_AXI_ETHERNET_DMA
    icmp_hdr = (ICMPHdr*) ((unsigned int) ehdr + sizeof(EthHdr) + sizeof(IPHdr));
    // Set the size of data to the one indicated in the IP header
    icmp_data_len = ntohs(iphdr->len) - sizeof(IPHdr) - sizeof(ICMPHdr);
#endif // USE_AXI_ETHERNET_DMA

#ifdef USE_AXI_ETHERNET_FIFO
    icmp_hdr = &(icmp_msg.hdr);
    // Set size of data to read to the size that remains in the RX FIFO
    icmp_data_len = arg - sizeof(EthHdr) - sizeof(IPHdr) - sizeof(ICMPHdr);

    // Copy ICMP Header and data from RX FIFO
    XLlFifo_Read(&(et->FifoInstance), (void*) icmp_hdr, (Xuint32) (sizeof(ICMPHdr) + icmp_data_len));

    // Set the size of data to the one indicated in the IP header
    icmp_data_len = ntohs(ip_hdr.len) - sizeof(IPHdr) - sizeof(ICMPHdr);
    if ((err = BufPool_GiveBuffer(et->bp, (void*) &eth_rep, AUTO_RETURNED)) < 0) {
        return (err);
    }
#endif // USE_AXI_ETHERNET_FIFO

    // Check for ICMP Echo request
    if (icmp_hdr->type == ICMP_ECHO_REQUEST) {
        et->rx_icmp++;

        if ((err = BufPool_GiveBuffer(et->bp, (void*) &eth_rep, AUTO_RETURNED)) < 0) {
            return (err);
        }

        // Put Ethernet header
        for (i = 0; i < 6; i++) {
            eth_rep->mac_to[i] = ehdr->mac_from[i];
            eth_rep->mac_from[i] = et->loc_mac[i];
        }
        eth_rep->tl = htons(ETH_TYPE_LEN_IP);

        // Fill IP Header
        ip_rep = (IPHdr*) ((unsigned int) eth_rep + sizeof(EthHdr));
        ip_rep->verlen = iphdr->verlen;
        ip_rep->tos = 0;
        ip_rep->len = htons(sizeof(IPHdr) + sizeof(ICMPHdr) + icmp_data_len);
        ip_rep->id = htons(iphdr->id);
        ip_rep->frag = htons(0);
        ip_rep->ttl = 0x80;
        ip_rep->proto = IP_PROTO_ICMP;
        for (i = 0; i < 4; i++) {
            ip_rep->srcip[i] = et->loc_ip[i];
            ip_rep->dstip[i] = iphdr->srcip[i];
        }
        ip_rep->chk = 0x0000;
        ip_rep->chk = ip_checksum((unsigned short*) ip_rep, sizeof(IPHdr));

        // Fill ICMP header
        icmp_rep = (ICMPHdr*) ((unsigned int) eth_rep + sizeof(EthHdr) + sizeof(IPHdr));
        icmp_rep->type = ICMP_ECHO_REPLY;
        icmp_rep->code = 0x00;
        icmp_rep->chk = ntohs(0x0000);

        // Fill ICMP data
        data_icmp = (unsigned char*) ((unsigned int) eth_rep + sizeof(EthHdr) + sizeof(IPHdr) + sizeof(ICMPHdr));
        icmp_data_src = (unsigned char*) ((unsigned int) icmp_hdr + sizeof(ICMPHdr));
        for (i = 0; i < icmp_data_len; i++) {
            *data_icmp = *icmp_data_src;
            data_icmp++;
            icmp_data_src++;
        }

        // Compute ICMP checksum
        icmp_rep->chk = ip_checksum((unsigned short*) icmp_rep, sizeof(ICMPHdr) + icmp_data_len);

        // Make Scatter-gather list with single item to send
        sgl[0].buf = (void*) eth_rep;
        sgl[0].len = sizeof(EthHdr) + sizeof(IPHdr) + sizeof(ICMPHdr) + icmp_data_len;
        sgl[0].flags = RETURN_BUFFER_WHEN_SENT;

        // Send Frame
        et->tx_ip_cnt++;
        et->tx_icmp++;
        return (Ethernet_SendFrame(et, &sgl[0], 1));
    } else {
        // Other types of ICMP messages are ignored
        et->rx_eth_drop++;
    }

    return (0);
}
