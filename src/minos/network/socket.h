/*******************************************************************************
                           Minos - Feminos/TCM
                           ___________________

 File:        socket.h

 Description: Definition of simplified socket interface.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  April 2013: created from code extracted in ethernet.h

*******************************************************************************/
#ifndef SOCKET_H
#define SOCKET_H

typedef enum { FREE,
               CLOSED,
               LISTEN,
               SYN_RCVD,
               ESTABLISHED,
               CLOSEWAIT } SocketState;

typedef struct _SockCtrlBlock {
    unsigned short loc_port;
    unsigned short rem_port;
    unsigned char rem_mac[6];
    unsigned char rem_ip[4];
    unsigned char proto;
    unsigned char state;
    unsigned int cur_seq_nb;
    unsigned int nxt_seq_nb;
    unsigned int ack_nb;
    unsigned short flags;
    short rep_size;
    int call_tcp_out;
} SockCtrlBlock;

void Socket_CtrlBlockClear(SockCtrlBlock* sa);

#endif // SOCKET_H
