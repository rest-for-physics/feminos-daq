/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        socket.c

 Description: Implementation of a simplified socket interface

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:

  April 2013: created from code extracted in ethernet_common.c

*******************************************************************************/

#include "platform_spec.h"
#include "socket_ui.h"
#include <stdio.h>

/*******************************************************************************
Socket_CtrlBlockClear
  Clears the SockCtrlBlock structure passed in the argument list
*******************************************************************************/
void Socket_CtrlBlockClear(SockCtrlBlock* sa) {
    int i;

    sa->loc_port = 0;
    sa->rem_port = 0;

    for (i = 0; i < 6; i++) {
        sa->rem_mac[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        sa->rem_ip[i] = 0;
    }
    sa->proto = 0;
    sa->cur_seq_nb = 0;
    sa->nxt_seq_nb = 0;
    sa->rep_size = 0;
    sa->call_tcp_out = 0;
    sa->state = FREE;
}

/*******************************************************************************
 Socket_Open
*******************************************************************************/
int Socket_Open(Ethernet* et, unsigned short port, unsigned char proto, SocketState state) {
    int i, sock;
    int done;

    // Try to find a free socket entry in the socket table
    done = 0;
    sock = -1;
    for (i = 0; i < MAX_NB_OF_SOCKS; i++) {
        if (et->sock_table[i].state == FREE) {
            done = 1;
            sock = i;
            break;
        }
    }

    // Allocate the new socket in the free entry
    if (done == 1) {
        et->sock_table[sock].loc_port = port;
        et->sock_table[sock].rem_port = 0;
        for (i = 0; i < 6; i++) {
            et->sock_table[sock].rem_mac[i] = 0;
        }
        for (i = 0; i < 4; i++) {
            et->sock_table[sock].rem_ip[i] = 0;
        }
        et->sock_table[sock].proto = proto;
        et->sock_table[sock].state = state;
        et->sock_cnt++;

        /*
        printf("Socket_Open: allocated sock(%d) loc_port=%d proto=%d socket_count=%d\n",
            sock,
            et->sock_table[sock].loc_port,
            et->sock_table[sock].proto,
            et->sock_cnt);
        */
    }

    return (sock);
}

/*******************************************************************************
 Socket_Close
*******************************************************************************/
int Socket_Close(Ethernet* et, int sock) {

    if (et->sock_table[sock].state == FREE) {
        return (0);
    } else {
        // Here we should free socket resources

        Socket_CtrlBlockClear(&(et->sock_table[sock]));

        et->sock_cnt--;

        // printf("Socket_Close: de-allocated sock(%d) socket_count=%d\n", sock, et->sock_cnt);
    }

    return (0);
}

/*******************************************************************************
 Socket_Send
 This function is called by the application to send user data to the supplied
 socket that contains the destination MAC, IP and protocol. The source MAC and
 source IP addresses are obtained from the Ethernet structure.
 The user offset must be 42 bytes exactly when sending in UDP
 The send function will prepend the Ethernet, IP and UDP or TCP headers in the
 user buffer before data
*******************************************************************************/
int Socket_Send(Ethernet* et, int to, void* data, short* sz) {
    EthHdr* eth_rep;
    IPHdr* ip_rep;
    UDPHdr* udp_rep;
    TCPHdr* tcp_rep;
    TCPPseudoHdr* ps_tcp_hdr;
    int i;
    SGListItem sgl;
    short sizeof_proto_hdr;
    unsigned short tcplen;

    // Check that the target is a valid socket
    if ((to < 0) || (to > et->sock_cnt)) {
        printf("Socket_Send: illegal socket %d\n", to);
        return (-1);
    }

    // Fill the protocol header
    if (et->sock_table[to].proto == IP_PROTO_UDP) {
        sizeof_proto_hdr = sizeof(UDPHdr);

        udp_rep = (UDPHdr*) ((unsigned int) data + sizeof(EthHdr) + sizeof(IPHdr));
        udp_rep->src_port = htons(et->sock_table[to].loc_port);
        udp_rep->dst_port = htons(et->sock_table[to].rem_port);
        udp_rep->len = htons(sizeof(UDPHdr) + (*sz));
        udp_rep->chk = 0x0000;
        sgl.flags = RETURN_BUFFER_WHEN_SENT;
        et->tx_udp++;
    } else if (et->sock_table[to].proto == IP_PROTO_TCP) {
        sizeof_proto_hdr = sizeof(TCPHdr);

        tcp_rep = (TCPHdr*) ((unsigned int) data + sizeof(EthHdr) + sizeof(IPHdr));
        tcp_rep->src_port = htons(et->sock_table[to].loc_port);
        tcp_rep->dst_port = htons(et->sock_table[to].rem_port);
        tcp_rep->seq_nb_h = htons((et->sock_table[to].cur_seq_nb >> 16) & 0xFFFF);
        tcp_rep->seq_nb_l = htons(et->sock_table[to].cur_seq_nb & 0xFFFF);
        tcp_rep->ack_nb_h = htons((et->sock_table[to].ack_nb >> 16) & 0xFFFF);
        tcp_rep->ack_nb_l = htons((et->sock_table[to].ack_nb & 0xFFFF));
        tcp_rep->flags = htons(et->sock_table[to].flags);
        tcp_rep->window = htons(65535);
        tcp_rep->checksum = htons(0x0000);
        tcp_rep->urgent_ptr = htons(0);

        // Prepare TCP pseudo-header
        ps_tcp_hdr = (TCPPseudoHdr*) ((unsigned int) tcp_rep - sizeof(TCPPseudoHdr));
        for (i = 0; i < 4; i++) {
            ps_tcp_hdr->srcip[i] = et->loc_ip[i];
            ps_tcp_hdr->dstip[i] = et->sock_table[to].rem_ip[i];
        }
        ps_tcp_hdr->reserved = 0x00;
        ps_tcp_hdr->proto = IP_PROTO_TCP;
        tcplen = sizeof(TCPHdr) + *sz;
        ps_tcp_hdr->tcplen = htons(tcplen);

        // Compute TCP checksum
        tcp_rep->checksum = ip_checksum((unsigned short*) ps_tcp_hdr, (tcplen + sizeof(TCPPseudoHdr)));

        sgl.flags = RETURN_BUFFER_WHEN_SENT; // RETURN_BUFFER_AFTER_REMOTE_ACK;

        et->sock_table[to].cur_seq_nb = et->sock_table[to].nxt_seq_nb;

        et->tx_tcp++;
    } else {
        printf("Socket_Send: unsupported protocol %d\n", et->sock_table[to].proto);
        return (-1);
    }

    // Fill IP Header
    ip_rep = (IPHdr*) ((unsigned int) data + sizeof(EthHdr));
    ip_rep->verlen = 0x45;
    ip_rep->tos = 0;
    ip_rep->len = htons(sizeof(IPHdr) + sizeof_proto_hdr + (*sz));
    ip_rep->id = htons(0);
    ip_rep->frag = htons(0);
    ip_rep->ttl = 0x80;
    ip_rep->proto = et->sock_table[to].proto;
    for (i = 0; i < 4; i++) {
        ip_rep->srcip[i] = et->loc_ip[i];
        ip_rep->dstip[i] = et->sock_table[to].rem_ip[i];
    }
    ip_rep->chk = 0x0000;
    ip_rep->chk = ip_checksum((unsigned short*) ip_rep, sizeof(IPHdr));

    // Fill Ethernet frame header
    eth_rep = (EthHdr*) data;
    for (i = 0; i < 6; i++) {
        eth_rep->mac_to[i] = et->sock_table[to].rem_mac[i];
        eth_rep->mac_from[i] = et->loc_mac[i];
    }
    eth_rep->tl = htons(ETH_TYPE_LEN_IP);

    // The length is that of headers + size of user data
    sgl.len = sizeof(EthHdr) + sizeof(IPHdr) + sizeof_proto_hdr + *sz;
    sgl.buf = (void*) ((unsigned int) eth_rep);

    // Send Frame
    et->tx_ip_cnt++;

    return (Ethernet_SendFrame(et, &sgl, 1));
}
