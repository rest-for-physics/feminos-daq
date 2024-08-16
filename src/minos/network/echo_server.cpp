/*******************************************************************************
                           Minos - Feminos/TCM
                           ___________________

 File:        echo_server.c

 Description: A simple echo server to test the UDP/IP interface
 This program listen to UDP/IP datagrams and echoes them to the sender.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  April 2006: created

  February 2013: revised to work with new API used in Minos

*******************************************************************************/

#include "ethernet.h"
#include "platform_spec.h"
#include "socket_ui.h"
#include <cstdio>

BufPool BufferPool;
Ethernet ether;

int main(void) {

    int status;
    short sz, size = 0;
    unsigned char* pdata_rx;
    unsigned char *tx_buf, *p;
    EthernetParam etp;
    int time_out;
    int done;
    int i;
    int lst_socket;

    printf("-- Entering main() --\n");

#ifdef TARGET_MICROBLAZE
    printf("Target platform is Mars MX2 / MicroBlaze\n");
#define TARGET_SOC_STANDALONE
#endif

#ifdef TARGET_SOC_STANDALONE
    etp.mtu = MAC_MAXIMUM_FRAME_SIZE_ETH;
    etp.des_speed = 100;
    etp.loc_mac[0] = 0x00;
    etp.loc_mac[1] = 0x01;
    etp.loc_mac[2] = 0x02;
    etp.loc_mac[3] = 0xaa;
    etp.loc_mac[4] = 0xbb;
    etp.loc_mac[5] = 0xcc;
    etp.loc_ip[0] = 192;
    etp.loc_ip[1] = 168;
    etp.loc_ip[2] = 10;
    etp.loc_ip[3] = 1;
#endif

    // Initialize Buffer Pool
    BufPool_Init(&BufferPool);
    etp.bp = &BufferPool;

    // Open the Ethernet interface
    if ((status = Ethernet_Open(&ether, &etp)) < 0) {
        printf("Ethernet_Open failed: status = %d\n", status);
        goto end;
    } else {
        printf("Ethernet_Open done\n");
    }

    // Get UDP datagrams
    done = 0;
    while (done == 0) {
        time_out = 0;
        while ((sz = Socket_Recv(&ether, &lst_socket, (void*) &pdata_rx, &size)) == 0) {
            time_out++;
            if (time_out > 100000000) {
                done = 1;
                printf("Socket_Recv timed out.\n");
                break;
            }
        }
        if (sz > 0) {
            // print received message for debug
            for (i = 0; i < size; i++) {
                printf("%c", *(pdata_rx + i));
            }
            printf("\n");

            // Get a buffer for ASCII response
            if ((status = BufPool_GiveBuffer(etp.bp, (void*) &tx_buf, AUTO_RETURNED)) < 0) {
                printf("%d BufPool_GiveBuffer failed\n", status);
                break;
            }

            // Leave space in the buffer for Ethernet IP and UDP headers
            p = (unsigned char*) tx_buf + USER_OFFSET_ETH_IP_UDP_HDR;
            // Zero the next two bytes which are reserved for future protocol
            *p = 0x00;
            p++;
            *p = 0x00;
            p++;

            // Copy received message to the transmit buffer
            for (i = 0; i < size; i++) {
                *(p + i) = *(pdata_rx + i);
            }

            // The response includes the two empty bytes we added
            size += 2;

            // Send echo message
            if ((sz = Socket_Send(&ether, lst_socket, (void*) tx_buf, &size)) < 0) {
                printf("Socket_Send failed with error code %d\n", sz);
                goto end;
            }
        } else {
            printf("Warning: Socket_Recv returned %d\n", sz);
        }
    }

    // Show statistics
    Ethernet_StatShow(&ether);

    // Close the Ethernet interface
    if ((status = Ethernet_Close(&ether)) < 0) {
        printf("Ethernet_Close failed: status = %d\n", status);
    }

end:

    printf("-- Exiting main() --\n");

    return (0);
}
