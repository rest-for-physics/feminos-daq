/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        ethernet_udp.c

 Description: UDP layer for MicroBlaze (Spartan 6) AXI Ethernet

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  November 2012: extracted from sockupd_common.c

*******************************************************************************/

#include "bufpool.h"
#include "ethernet.h"
#include "platform_spec.h"
#include "socket_ui.h"
#include <stdio.h>

#include "xbasic_types.h"
#include "xparameters.h"

/*******************************************************************************
 Ethernet_ProcessUDP
 This function is called internally whenever an UDP frame is received.
 User data is copied to the buffer supplied and the datagram size is returned
 in addition to an index to an internal socket table to identify the remote
 partner
*******************************************************************************/
int Ethernet_ProcessUDP(Ethernet* et, EthHdr* ehdr, IPHdr* iphdr, unsigned int arg, int* from, void** data) {
    int i, sock, open_sock;
    UDPHdr* pudp_hdr;
    short udp_data_sz;
    unsigned short loc_port, rem_port;

#ifdef USE_AXI_ETHERNET_DMA
    pudp_hdr = (UDPHdr*) ((unsigned int) ehdr + sizeof(EthHdr) + sizeof(IPHdr));
#endif // USE_AXI_ETHERNET_DMA

#ifdef USE_AXI_ETHERNET_FIFO

    // Copy UDP Header from LLTemac RX FIFO
    XLlFifo_Read(&(set->FifoInstance), (void*) &udp_hdr, (Xuint32) (sizeof(UDPHdr)));
    pudp_hdr = &udp_hdr;
#endif // USE_AXI_ETHERNET_FIFO

    // Get source/destination port
    loc_port = ntohs(pudp_hdr->dst_port);
    rem_port = ntohs(pudp_hdr->src_port);

    // Try to map this datagram to an existing socket
    sock = -1;
    open_sock = 0;
    for (i = 0; i < MAX_NB_OF_SOCKS; i++) {
        // We only make comparison on the target port. In principle, we should also match
        // the source port. But this avoids opening a new socket every time the client side
        // program is re-started. We do not check that all opened sockets are alive and
        // need to avoid a lack of socket descriptors if clients do not inform us that a
        // socket can be closed.
        if ((et->sock_table[i].proto == IP_PROTO_UDP) &&
            (et->sock_table[i].state == ESTABLISHED) &&
            (et->sock_table[i].loc_port == loc_port)) {
            sock = i;
            open_sock = 0;
            break;
        }
        // We check if a socket was opened to listen to "any port"
        else if ((et->sock_table[i].proto == IP_PROTO_UDP) &&
                 (et->sock_table[i].state == LISTEN) &&
                 (et->sock_table[i].loc_port == UDP_PORT_ANY)) {
            sock = i;
            open_sock = 1;
            // we must no break here because there may be another
            // socket in the array beyond this one that has a matching
            // destination port and is in the ESTABLISHED state
        }
    }
    // Dedicate the matching socket to the newly established communication with the client host
    // Allocate a new socket to keep one of them in listen to "any port"
    if (open_sock) {
        // Save the target port
        et->sock_table[sock].loc_port = loc_port;

        // Save the MAC address of the remote client
        for (i = 0; i < 6; i++) {
            et->sock_table[sock].rem_mac[i] = ehdr->mac_from[i];
        }

        // Save the IP address of the remote client
        for (i = 0; i < 4; i++) {
            et->sock_table[sock].rem_ip[i] = iphdr->srcip[i];
        }

        // Move the socket to the established state
        et->sock_table[sock].state = ESTABLISHED;

        // Open a new socket on UDP "port any"
        if ((open_sock = Socket_Open(et, UDP_PORT_ANY, IP_PROTO_UDP, LISTEN)) < 0) {
            *from = -1;
            return (open_sock);
        }
    }
    // No matching socket was found
    else if (sock == -1) {
        // Drop that frame silently
        et->rx_udp_drop++;
        return (0);
    }
    *from = sock;

    // Save the source port of the remote client on each datagram received
    // This allows that the same socket is used after the client program is
    // restarted even if it did not close the socket of the server on exit.
    et->sock_table[sock].rem_port = rem_port;

    /*
    printf("udp.src_port = 0x%x\n", ntohs(pudp_hdr->src_port));
    printf("udp.dst_port = 0x%x\n", ntohs(pudp_hdr->dst_port));
    printf("udp.len      = %d\n", ntohs(pudp_hdr->len));
    printf("udp.check    = 0x%x\n", ntohs(pudp_hdr->chk));
    */

    udp_data_sz = ntohs(pudp_hdr->len) - sizeof(UDPHdr);

#ifdef USE_AXI_ETHERNET_DMA
    *data = (void*) ((unsigned int) pudp_hdr + sizeof(UDPHdr));
#endif // USE_AXI_ETHERNET_DMA

#ifdef USE_AXI_ETHERNET_FIFO
    // Set size of data to read to the size that remains in the RX FIFO
    udp_data_sz = arg - sizeof(EthHdr) - sizeof(IPHdr) - sizeof(UDPHdr);

    // Copy UDP data from the RX FIFO
    XLlFifo_Read(&(et->FifoInstance), (void*) (&(su->rx_frame[0])), (Xuint32) (udp_data_sz));

    // Set the "effective" UDP data size to the value indicated in the IP header
    udp_data_sz = udp_hdr.len - sizeof(UDPHdr);
    *data = &(et->rx_frame[0]);
#endif // USE_AXI_ETHERNET_FIFO

    et->rx_udp++;
    return (udp_data_sz);
}
