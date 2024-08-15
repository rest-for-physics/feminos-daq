/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        ethernet_tcp.c

 Description: TCP layer interface for MicroBlaze (Spartan 6) AXI Ethernet

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  November 2012: created

*******************************************************************************/

#include "bufpool.h"
#include "ethernet.h"
#include "platform_spec.h"
#include "socket_ui.h"
#include <stdio.h>

#include "xbasic_types.h"
#include "xparameters.h"

/*******************************************************************************
 tcp_header_decode_flags
 Converts the flags of a TCP header to string
*******************************************************************************/
void tcp_header_decode_flags(unsigned short flags, char* str) {
    *str = '\0';
    if (flags & TCP_FLAG_FIN) strcat(str, "FIN ");
    if (flags & TCP_FLAG_SYN) strcat(str, "SYN ");
    if (flags & TCP_FLAG_RST) strcat(str, "RST ");
    if (flags & TCP_FLAG_PSH) strcat(str, "PSH ");
    if (flags & TCP_FLAG_URG) strcat(str, "URG ");
    if (flags & TCP_FLAG_ACK) strcat(str, "ACK ");
    if (flags & TCP_FLAG_ECN) strcat(str, "ECN ");
}

/*******************************************************************************
 SockTCP_Listen
*******************************************************************************/
int SockTCP_Listen(Ethernet* et, int sock, EthHdr* ehdr, IPHdr* iphdr, unsigned int arg, int* from, void** data) {
    int i;
    TCPHdr* ptcp_hdr;
    unsigned short loc_port;
    int the_sock;

    printf("SockTCP_Listen:\n");

    ptcp_hdr = (TCPHdr*) ((unsigned int) ehdr + sizeof(EthHdr) + sizeof(IPHdr));

    // In the LISTEN state only segments with the SYN flag are acceptable
    if ((ptcp_hdr->flags & TCP_FLAG_SYN) == 0) {
        the_sock = sock;

        // Make RESET
        et->sock_table[the_sock].flags = TCP_PUT_HDR_LEN(TCP_FLAG_RST, sizeof(TCPHdr));
        et->sock_table[the_sock].rep_size = 0;
    } else {
        // Try to open a new socket
        {
            loc_port = et->sock_table[sock].loc_port;
            if ((the_sock = Socket_Open(et, loc_port, IP_PROTO_TCP, CLOSED)) < 0) {
                printf("SockTCP_Listen: Socket_Open failed: %d\n", the_sock);
                return (the_sock);
            }
        }

        // Get the acknowledge number knowing that SYN count for 1 byte
        et->sock_table[the_sock].ack_nb = (ptcp_hdr->seq_nb_h << 16) | ptcp_hdr->seq_nb_l;
        et->sock_table[the_sock].ack_nb++;

        // Derive our local initial sequence number from sender's sequence number
        et->sock_table[the_sock].cur_seq_nb = ~et->sock_table[the_sock].ack_nb;
        et->sock_table[the_sock].nxt_seq_nb = et->sock_table[the_sock].cur_seq_nb;

        // Make flags
        et->sock_table[the_sock].flags = TCP_PUT_HDR_LEN((TCP_FLAG_SYN | TCP_FLAG_ACK), sizeof(TCPHdr));
        et->sock_table[the_sock].rep_size = 0;

        // Change the state of the newly created socket to SYN_RCVD
        et->sock_table[the_sock].state = SYN_RCVD;
        et->sock_table[the_sock].nxt_seq_nb++;
    }

    // Save remote MAC address
    for (i = 0; i < 6; i++) {
        et->sock_table[the_sock].rem_mac[i] = ehdr->mac_from[i];
    }

    // Save remote IP address
    for (i = 0; i < 4; i++) {
        et->sock_table[the_sock].rem_ip[i] = iphdr->srcip[i];
    }

    // Save the remote source port
    et->sock_table[the_sock].rem_port = ptcp_hdr->src_port;

    // Tell that we need an output frame for this socket
    et->sock_table[the_sock].call_tcp_out++;
    et->call_tcp_out++;

    return (0);
}

/*******************************************************************************
 SockTCP_SynRcvd
*******************************************************************************/
int SockTCP_SynRcvd(Ethernet* et, int sock, EthHdr* ehdr, IPHdr* iphdr, unsigned int arg, int* from, void** data) {
    TCPHdr* ptcp_hdr;

    printf("SockTCP_SynRcvd:\n");

    // Check we received an ACK
    ptcp_hdr = (TCPHdr*) ((unsigned int) ehdr + sizeof(EthHdr) + sizeof(IPHdr));
    if (ptcp_hdr->flags & TCP_FLAG_ACK) {
        et->sock_table[sock].state = ESTABLISHED;
    } else {
        et->sock_table[sock].state = LISTEN;
    }

    return (0);
}

/*******************************************************************************
 SockTCP_Established
*******************************************************************************/
int SockTCP_Established(Ethernet* et, int sock, EthHdr* ehdr, IPHdr* iphdr, unsigned int arg, int* from, void** data) {
    TCPHdr* ptcp_hdr;
    unsigned int seq_nb, ack_nb;
    unsigned short data_len;

    printf("SockTCP_Established:\n");

    //
    ptcp_hdr = (TCPHdr*) ((unsigned int) ehdr + sizeof(EthHdr) + sizeof(IPHdr));
    seq_nb = (ptcp_hdr->seq_nb_h << 16) | ptcp_hdr->seq_nb_l;
    ack_nb = (ptcp_hdr->ack_nb_h << 16) | ptcp_hdr->ack_nb_l;

    data_len = ntohs(iphdr->len) - sizeof(IPHdr) - sizeof(TCPHdr);
    printf("SockTCP_Established: received %d bytes\n", data_len);

    et->sock_table[sock].flags = 0;
    if (ptcp_hdr->flags & TCP_FLAG_FIN) {
        et->sock_table[sock].flags = TCP_FLAG_FIN | TCP_FLAG_ACK;
        et->sock_table[sock].nxt_seq_nb += 1;
        et->sock_table[sock].ack_nb += 1;
        et->sock_table[sock].state = CLOSEWAIT;
    }
    if (data_len) {
        et->sock_table[sock].ack_nb += data_len;
        et->sock_table[sock].flags |= TCP_FLAG_ACK;
    }

    if (et->sock_table[sock].flags == 0) return (0);

    // Finish flags
    et->sock_table[sock].flags = TCP_PUT_HDR_LEN(et->sock_table[sock].flags, sizeof(TCPHdr));

    et->sock_table[sock].rep_size = 0;
    et->sock_table[sock].nxt_seq_nb += 0;

    // Tell that we need an output frame for this socket
    et->sock_table[sock].call_tcp_out++;
    et->call_tcp_out++;

    return (0);
}

/*******************************************************************************
 SockTCP_CloseWait
*******************************************************************************/
int SockTCP_CloseWait(Ethernet* et, int sock, EthHdr* ehdr, IPHdr* iphdr, unsigned int arg, int* from, void** data) {
    TCPHdr* ptcp_hdr;

    printf("SockTCP_CloseWait:\n");

    // Check we received an ACK
    ptcp_hdr = (TCPHdr*) ((unsigned int) ehdr + sizeof(EthHdr) + sizeof(IPHdr));
    if (ptcp_hdr->flags & TCP_FLAG_ACK) {
        et->sock_table[sock].state = CLOSED;
        et->sock_table[sock].flags = TCP_FLAG_ACK;
        et->sock_table[sock].nxt_seq_nb++;
    } else {
        et->sock_table[sock].state = CLOSED;
    }

    return (0);
}
/*******************************************************************************
 SockTCP_Dispatch
*******************************************************************************/
int SockTCP_Dispatch(Ethernet* et, int sock, EthHdr* ehdr, IPHdr* iphdr, unsigned int arg, int* from, void** data) {
    // Call the appropriate function according to socket state
    switch (et->sock_table[sock].state) {
        case CLOSED:
            printf("SockTCP_Dispatch: callback for CLOSED state\n");
            break;

        case LISTEN:
            return (SockTCP_Listen(et, sock, ehdr, iphdr, arg, from, data));
            break;

        case SYN_RCVD:
            return (SockTCP_SynRcvd(et, sock, ehdr, iphdr, arg, from, data));
            break;

        case ESTABLISHED:
            return (SockTCP_Established(et, sock, ehdr, iphdr, arg, from, data));
            break;

        case CLOSEWAIT:
            return (SockTCP_CloseWait(et, sock, ehdr, iphdr, arg, from, data));
            break;

        default:
            printf("SockTCP_Dispatch: callback for unknown state\n");
    }

    return (0);
}

/*******************************************************************************
 Ethernet_ProcessTCP
 This function is called internally whenever a TCP frame is received.
*******************************************************************************/
int Ethernet_ProcessTCP(Ethernet* et, EthHdr* ehdr, IPHdr* iphdr, unsigned int arg, int* from, void** data) {
    int i;
    TCPHdr* ptcp_hdr;
    char str[80];
    int tcphlen;
    unsigned short* push;
    unsigned int seq_nb, ack_nb;
    int sock_part;

#ifdef USE_AXI_ETHERNET_DMA
    ptcp_hdr = (TCPHdr*) ((unsigned int) ehdr + sizeof(EthHdr) + sizeof(IPHdr));
#endif // USE_AXI_ETHERNET_DMA

#ifdef USE_AXI_ETHERNET_FIFO

    // Copy TCP Header from LLTemac RX FIFO
    XLlFifo_Read(&(et->FifoInstance), (void*) &tcp_hdr, (Xuint32) (sizeof(TCPHdr)));
    ptcp_hdr = &tc_hdr;
#endif // USE_AXI_ETHERNET_FIFO

    // Convert source/destination ports to host byte order
    ptcp_hdr->dst_port = ntohs(ptcp_hdr->dst_port);
    ptcp_hdr->src_port = ntohs(ptcp_hdr->src_port);

    // Convert Flag/len to host byte order
    ptcp_hdr->flags = ntohs(ptcp_hdr->flags);
    tcphlen = TCP_GET_HDR_LEN(ptcp_hdr->flags);
    tcp_header_decode_flags(ptcp_hdr->flags, &str[0]);

    // Convert sequence number and ack number to host byte order
    ptcp_hdr->seq_nb_h = ntohs(ptcp_hdr->seq_nb_h);
    ptcp_hdr->seq_nb_l = ntohs(ptcp_hdr->seq_nb_l);
    ptcp_hdr->ack_nb_h = ntohs(ptcp_hdr->ack_nb_h);
    ptcp_hdr->ack_nb_l = ntohs(ptcp_hdr->ack_nb_l);

    seq_nb = (ptcp_hdr->seq_nb_h << 16) | ptcp_hdr->seq_nb_l;
    ack_nb = (ptcp_hdr->ack_nb_h << 16) | ptcp_hdr->ack_nb_l;
    /*
    printf("Received TCP datagram:\n");
    printf("src_port = 0x%04x      dst_port   = 0x%04x\n", ptcp_hdr->src_port, ptcp_hdr->dst_port);
    printf("seq_nb   = 0x%08x  ack_nb     = 0x%08x\n", seq_nb, ack_nb);
    printf("flags    = 0x%04x      %s\n", ptcp_hdr->flags, str);
    printf("window   = 0x%04x\n", ntohs(ptcp_hdr->window));
    printf("checksum = 0x%04x      urgent_ptr = 0x%04x\n", ntohs(ptcp_hdr->checksum), ntohs(ptcp_hdr->urgent_ptr));
    push = (unsigned short *) ((unsigned int)ptcp_hdr + sizeof(TCPHdr));
    for (i=0; i<((tcphlen-20)/2); i++)
    {
        printf("opt(%d)   = 0x%04x\n", i, ntohs(*push));
        push++;
    }
    */
    // Try to direct this datagram to an existing socket
    sock_part = -1;
    for (i = 0; i < MAX_NB_OF_SOCKS; i++) {
        if ((et->sock_table[i].state != FREE) && (et->sock_table[i].loc_port == ptcp_hdr->dst_port)) {
            // Partial match is for server socket
            if (et->sock_table[i].state == LISTEN) {
                sock_part = i;
            }

            // Full match is for a previously established connection
            if ((et->sock_table[i].rem_port == ptcp_hdr->src_port) &&
                (iphdr->srcip[0] == et->sock_table[i].rem_ip[0]) &&
                (iphdr->srcip[1] == et->sock_table[i].rem_ip[1]) &&
                (iphdr->srcip[2] == et->sock_table[i].rem_ip[2]) &&
                (iphdr->srcip[3] == et->sock_table[i].rem_ip[3])) {
                return (SockTCP_Dispatch(et, i, ehdr, iphdr, arg, from, data));
            }
        }
    }

    if (sock_part >= 0) {
        return (SockTCP_Dispatch(et, sock_part, ehdr, iphdr, arg, from, data));
    } else {
        et->rx_tcp_drop++;
        printf("Ethernet_ProcessTCP: dropped datagram src_port = 0x%04x dst_port = 0x%04x\n", ptcp_hdr->src_port, ptcp_hdr->dst_port);
    }

    return (0);
}

/*******************************************************************************
 Ethernet_OutputTCP
 This function is called periodically from the main application loop to perform
 the output process of TCP/IP
*******************************************************************************/
int Ethernet_OutputTCP(Ethernet* et) {
    int i;
    int err;
    int sock;
    EthHdr* eth_rep;

    // See if action is needed
    if (et->call_tcp_out == 0) {
        return (0);
    }

    // Find the socket that requires action
    sock = -1;
    for (i = 0; i < MAX_NB_OF_SOCKS; i++) {
        if (et->sock_table[i].call_tcp_out > 0) {
            sock = i;
            break;
        }
    }

    // If action is required but no socket says so, there must be an error
    if (sock == -1) {
        printf("Ethernet_OutputTCP: Warning %d actions to perform but no socket found\n", et->call_tcp_out);
        if (et->call_tcp_out > 1) {
            et->call_tcp_out--;
        } else {
            et->call_tcp_out = 0;
        }
        return (0);
    }

    // Get a buffer for sending a response
    if ((err = BufPool_GiveBuffer(et->bp, (void*) &eth_rep, AUTO_RETURNED)) < 0) {
        return (err);
    }

    // Decrement action pending count on this socket and the global count
    et->sock_table[sock].call_tcp_out--;
    et->call_tcp_out--;

    // Send the frame
    return (Socket_Send(et, sock, eth_rep, &(et->sock_table[sock].rep_size)));
}
