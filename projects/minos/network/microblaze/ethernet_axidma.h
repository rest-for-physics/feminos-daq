/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        ethernet_axidma.h

 Description: Definition related to Soft Tri-mode Ethernet MAC (TEMAC) with DMA
 Intended to be used on Spartan-6 device

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  May 2011: created

  October 2012: added field return_policy to BufOwner structure

  April 2013: file sockudp_axi_dma.h renamed to ethernet_axidma.h

*******************************************************************************/
#ifndef ETHERNET_AXIDMA_H
#define ETHERNET_AXIDMA_H

#include "socket.h"
#include "xaxidma.h"
#include "xaxiethernet.h"
#include "xintc.h"

#define MAX_NB_OF_SOCKS 8

#define MAC_MINIMUM_FRAME_SIZE 64

#define PENDING_ACK_STACK_SIZE 32

// Always reserve buffer space for biggest supported Jumbo frame
// #ifdef HAS_JUMBO_FRAMES
#define MAC_MAXIMUM_FRAME_SIZE_GBE 8192 // jumbo frames
// #else
#define MAC_MAXIMUM_FRAME_SIZE_ETH 1536 // standard frames
// #endif
#define RXBD_CNT 32                               /* Number of RxBDs to use */
#define TXBD_CNT 16                               /* Number of TxBDs to use */
#define BD_ALIGNMENT XAXIDMA_BD_MINIMUM_ALIGNMENT /* Byte alignment of BDs */

#define RXBD_SPACE_BYTES XAxiDma_BdRingMemCalc(BD_ALIGNMENT, RXBD_CNT)
#define TXBD_SPACE_BYTES XAxiDma_BdRingMemCalc(BD_ALIGNMENT, TXBD_CNT)

typedef struct _BufOwner {
    unsigned int buffer_min;
    unsigned int buffer_max;
    void* owner;
    void (*buffer_return)(void* ow, unsigned int badr);
} BufOwner;

typedef struct _Ethernet {
    /* Buffer Descriptor Rings */
    char RxBdSpace[RXBD_SPACE_BYTES] __attribute__((aligned(BD_ALIGNMENT)));
    char TxBdSpace[TXBD_SPACE_BYTES] __attribute__((aligned(BD_ALIGNMENT)));

    /* Buffer for receiving frames */
    unsigned char rx_frame[RXBD_CNT][MAC_MAXIMUM_FRAME_SIZE_GBE] __attribute__((aligned(BD_ALIGNMENT)));

    /* Hardware devices */
    XAxiEthernet xaxie;
    XAxiDma xaxidma;
    XIntc xintc;

    XAxiDma_Bd* CurRxBdPtr;

    /* Interface MAC and IP */
    unsigned char loc_mac[6];
    unsigned char loc_ip[4];

    /* Interface parameters */
    unsigned int mtu;
    int des_speed;
    int actual_speed;
    int isduplex;

    /* Network and Protocol Statistics */
    int rx_eth_fr_cnt;
    int rx_eth_drop;
    int tx_eth_fr_cnt;
    int rx_arp_req_cnt;
    int tx_arp_rep_cnt;
    int rx_ip_cnt;
    int rx_ip_drop;
    int tx_ip_cnt;
    int rx_icmp;
    int rx_icmp_drop;
    int tx_icmp;
    int rx_udp;
    int rx_udp_drop;
    int tx_udp;
    int rx_tcp;
    int rx_tcp_drop;
    int tx_tcp;

    /* Socket Table */
    int call_tcp_out;
    SockCtrlBlock sock_table[MAX_NB_OF_SOCKS];
    int sock_cnt;

    /* Buffer owner array */
    BufOwner bown[4];
    int bown_cnt;
    BufPool* bp;
} Ethernet;

#define ERR_LLTEMAC_FAILURE -200
#define ERR_LLTEMAC_UNSPEC_ERR -201
#define ERR_LLTEMAC_NO_DATA -202
#define ERR_LLTEMAC_DEV_STARTED -204
#define ERR_LLTEMAC_IPIF_ERROR -207
#define ERR_LLTEMAC_FIFO_NO_ROOM -208
#define ERR_LLTEMAC_PHY_OP_FAILED -210
#define ERR_LLTEMAC_EXCESSIVE_DATA -211
#define ERR_LLTEMAC_ILL_PARAM -212
#define ERR_BOWN_ARRAY_FULL -213
#define ERR_BOWN_NOT_FOUND -214

#endif // ETHERNET_AXIDMA_H
