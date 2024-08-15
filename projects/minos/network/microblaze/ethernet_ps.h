/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        ethernet_ps.h

 Description: Definition of a minimal Ethernet interface
 Intended to be used on a MicroBlaze Spartan 6

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2010: copied from T2K files and adapted to Spartan 6 / AXI

  October 2012: removed IP common structure definitions and constants to put
  them in basic_ip.h

  April 2013: file sockudp_ps.h renamed to ethernet_ps.h

*******************************************************************************/
#ifndef ETHERNET_PS_H
#define ETHERNET_PS_H

#include "basic_ip.h"
#include "bufpool.h"

#ifdef USE_AXI_ETHERNET_DMA
#include "ethernet_axidma.h"
#endif // USE_AXI_ETHERNET_DMA

#ifdef USE_AXI_ETHERNET_FIFO
#include "xaxiethernet.h"
#include "xllfifo.h"

#define MAC_MINIMUM_FRAME_SIZE 64
#define MAC_MAXIMUM_FRAME_SIZE 1500

typedef struct _Ethernet {
    XAxiEthernet xaxie;
    XLlFifo FifoInstance;
    unsigned char loc_mac[6];
    unsigned char loc_ip[4];
    int rx_eth_fr_cnt;
    int tx_eth_fr_cnt;
    int rx_arp_req_cnt;
    int tx_arp_rep_cnt;
    int rx_ip_cnt;
    int rx_ip_err;
    int rx_udp;
    int rx_icmp;
    int tx_ip_cnt;
    int tx_icmp;
    int tx_udp;
    unsigned char rem_mac[6];
    unsigned char rem_ip[4];
    unsigned short loc_port;
    unsigned short rem_port;
    unsigned char rx_frame[MAC_MAXIMUM_FRAME_SIZE];
    int actual_speed;
    int isduplex;
    BufPool* bp;
} Ethernet;
#endif // USE_AXI_ETHERNET_FIFO

typedef struct _SGListItem {
    void* buf;
    short len;
    unsigned int flags;
} SGListItem;

typedef struct _Ethernet_Param {
    unsigned int mtu;
    int des_speed;
    unsigned char loc_mac[6];
    unsigned char loc_ip[4];
    BufPool* bp;
} EthernetParam;

#define SIZEOF_ARP_REPLY_FRAME (((sizeof(EthHdr) + sizeof(Arp_ReqRep))) > (MAC_MINIMUM_FRAME_SIZE) ? ((sizeof(EthHdr) + sizeof(Arp_ReqRep))) : (MAC_MINIMUM_FRAME_SIZE))

#define USER_OFFSET_ETH_IP_UDP_HDR 42

// Definition of Buffer return policies
#define RETURN_BUFFER_WHEN_SENT 1
#define RETURN_BUFFER_AFTER_REMOTE_ACK 2

// Function prototypes
unsigned short ip_checksum(unsigned short* buf, unsigned short len);

int Ethernet_InitPhy(Ethernet* et);
int Ethernet_ProcessARP(Ethernet* et, unsigned int arg);
int Ethernet_ProcessIP(Ethernet* et, EthHdr* ehdr, unsigned int ip_base, int* from, void** data);
int Ethernet_ProcessICMP(Ethernet* et, EthHdr* ehdr, IPHdr* iphdr, unsigned int arg);
int Ethernet_ProcessUDP(Ethernet* et, EthHdr* ehdr, IPHdr* iphdr, unsigned int arg, int* from, void** data);
int Ethernet_ProcessTCP(Ethernet* et, EthHdr* ehdr, IPHdr* iphdr, unsigned int arg, int* from, void** data);
int Ethernet_SendFrame(Ethernet* et, SGListItem* sgl, int num_el);
int Ethernet_AddBufferOwnerCallBack(Ethernet* et, void* ow, unsigned int buf_min, unsigned int buf_max, void (*retfun)(void* ow, unsigned int badr));
int Ethernet_ReturnBuffer(Ethernet* et, unsigned int buf);
int Ethernet_CheckTx(Ethernet* et);
int Ethernet_OutputTCP(Ethernet* et);

#endif // ETHERNET_PS_H
