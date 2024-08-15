/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        basic_ip.h

 Description: Basic definitions of structure and constants for IP networking
  with Ethernet

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2012: created by extraction from sockudp_ps.h

*******************************************************************************/
#ifndef _BASIC_IP_H
#define _BASIC_IP_H

/* Ethernet Header */
typedef struct _EthHdr {
    unsigned char mac_to[6];
    unsigned char mac_from[6];
    unsigned short tl; // Ethernet type/length
} EthHdr;

/* Type/Length definitions */
#define ETH_TYPE_LEN_ARP 0x0806
#define ETH_TYPE_LEN_IP 0x0800

/* ARP Request or Response */
typedef struct _Arp_ReqRep {
    unsigned short ar_hrd;   // Hardware type. Ethernet = 0x0001
    unsigned short ar_pro;   // protocol type. IP = 0x0800
    unsigned char ar_hln;    // Ethernet address size = 6
    unsigned char ar_pln;    // IP address size = 4
    unsigned short ar_op;    // ARP opcode. REQUEST = 0x0001, REPLY = 0x0002
    unsigned char ar_sha[6]; // Sender's hardware address
    unsigned char ar_sip[4]; // Sender's IP Address
    unsigned char ar_tha[6]; // Target hardware address
    unsigned char ar_tip[4]; // Target IP address
} Arp_ReqRep;

/* ARP Opcode Definition */
#define ARP_REQUEST 0x0001
#define ARP_REPLY 0x0002

/* IP Header */
typedef struct _IPHdr {
    unsigned char verlen;
    unsigned char tos;
    unsigned short len;
    unsigned short id;
    unsigned short frag;
    unsigned char ttl;
    unsigned char proto;
    unsigned short chk;
    unsigned char srcip[4];
    unsigned char dstip[4];
} IPHdr;

/* IP Protocols (very few of them) */
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17

/* Ports (not all standards) */
#define UDP_PORT_ANY 0

/* ICMP Header */
typedef struct _ICMPHdr {
    unsigned char type;
    unsigned char code;
    unsigned short chk;
} ICMPHdr;

/* ICMP Message */
typedef struct _ICMPMsg {
    ICMPHdr hdr;
    unsigned char data[1024];
} ICMPMsg;

/* ICMP Types */
#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY 0

/* UDP Header */
typedef struct _UDPHdr {
    unsigned short src_port;
    unsigned short dst_port;
    unsigned short len;
    unsigned short chk;
} UDPHdr;

/* TCP Header */
typedef struct _TCPHdr {
    unsigned short src_port;
    unsigned short dst_port;
    unsigned short seq_nb_h;
    unsigned short seq_nb_l;
    unsigned short ack_nb_h;
    unsigned short ack_nb_l;
    unsigned short flags;
    unsigned short window;
    unsigned short checksum;
    unsigned short urgent_ptr;
} TCPHdr;

/* TCP Pseudo Header */
typedef struct _TCPPseudoHdr {
    unsigned char srcip[4];
    unsigned char dstip[4];
    unsigned char reserved;
    unsigned char proto;
    unsigned short tcplen;
} TCPPseudoHdr;

/* TCP flags */
#define TCP_FLAG_FIN 0x0001
#define TCP_FLAG_SYN 0x0002
#define TCP_FLAG_RST 0x0004
#define TCP_FLAG_PSH 0x0008
#define TCP_FLAG_ACK 0x0010
#define TCP_FLAG_URG 0x0020
#define TCP_FLAG_ECN 0x0040

#define TCP_GET_HDR_LEN(w) (((w) & 0xF000) >> 10)
#define TCP_PUT_HDR_LEN(w, len) (((w) & 0x0FFF) | ((len & 0x003C) << 10))

/* Function prototypes */
unsigned short ip_checksum(unsigned short* buf, unsigned short len);

#endif
