/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        data_server.c

 Description: A simple data server to test a UDP/IP interface
 This program listens to requests sent over UDP/IP and sends back
 binary data to the sender.

 The format of the request is an ASCII line:
 areq <size> <mode> 0 <ix_first> <ix_last>
 Where:
 - size is the requested data size in bytes per frame
 - mode specifies options on where to take the data to send
 - the number of frames sent for one request is (ix_last-ix_first+1)

 This application works with AUTO_RETURNED buffers.
 For debug, it can also run with USER_RETURNED buffers but the application
 must guarantee that the buffer it is trying to free has been effectively
 sent over Ethernet. A simple printout to the RS232 after calling SockUDP_Send
 seems sufficient.

 Platform seetings:
 . Virtex 4 mini-module PowerPC405@300MHz, PLB@100Mhz
 . Cache enabled
 . Tri-mode Ethernet MAC
 . Custom UDP/IP driver
 . standalone application (no operating system)

 Client: Windows XP, Pentium 4 3 Ghz, NIC DLink GBE-530T

 Measurement conditions:
 . 64 frames sent upon each request, 1460 byte per frame

 Typical performance:
 . without filling buffers with data: 37 MByte/s sustained
 . buffers filled by processor with integers: 26 MByte/s sustained
 . buffers filled by processor with 32-bit data read over PLB: 16 MByte/s sustained

 With Jumbo frames enabled:
 . 16 frames per request
 . without filling buffers:
   MTU (Byte) Speed (MByte/s)
   8060       90
    6060       71
    4060       64
    2060       48
    1460       37
    1060       29
. buffers filled by processor with 32-bit data read over PLB: 16 MByte/s
 (i.e. no improvement with Jumbo frames; processor I/O is limiting)


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2010: created

*******************************************************************************/

#include "ethernet.h"
#include "platform_spec.h"
#include <cstdio>

// Macros for reply header
#define FRAME_HDR_V2 0x4000
#define FRAME_TYPE_FEM_DATA 0x0000
#define FRAME_TYPE_DCC_DATA 0x0001
#define FRAME_FLAG_EORQ 0x0004
#define FRAME_FLAG_EOEV 0x0008
#define PUT_FRAME_TY_V2(word, ty) (((word) & 0x03FF) | FRAME_HDR_V2 | (((ty) & 0x000F) << 10))

BufPool BufferPool;
SockUDP su;

int main(void) {

    int status;
    short sz, size = 0;
    unsigned char* pdata_rx;
    unsigned char* tx_buf;
    unsigned char* old_tx_buf;
    SockUDPParam sup;
    int time_out;
    int done;
    int i, j;
    int req_size;
    int req_cnt;
    int req_opt;
    int req_end;
    int req_beg;
    unsigned short hdr;
    unsigned int signal_dbg;
    volatile unsigned int* port_dbg;

    xil_printf("-- Entering main() --\r\n");

#ifdef WIN32
    xil_printf("Target platform is Windows\r\n");
    sup.server_port = 1122;
    sup.set_nbio = 0;
#endif

#ifdef TARGET_PPC405
    xil_printf("Target platform is PowerPC 405\r\n");
#define TARGET_SOC_STANDALONE
#endif

#ifdef TARGET_ML507
    xil_printf("Target platform is ML507 / PowerPC 440\r\n");
#define TARGET_SOC_STANDALONE
#endif

#ifdef TARGET_ML523
    xil_printf("Target platform is ML523 / PowerPC 440\r\n");
#define TARGET_SOC_STANDALONE
#endif

#ifdef TARGET_SOC_STANDALONE
    XCache_EnableICache(0x80000000);
    XCache_EnableDCache(0x80000000);

    sup.loc_mac[0] = 0x00;
    sup.loc_mac[1] = 0x01;
    sup.loc_mac[2] = 0x02;
    sup.loc_mac[3] = 0xaa;
    sup.loc_mac[4] = 0xbb;
    sup.loc_mac[5] = 0xcc;
    sup.loc_ip[0] = 192;
    sup.loc_ip[1] = 168;
    sup.loc_ip[2] = 10;
    sup.loc_ip[3] = 12;
#endif

#ifdef VXWORKS
    xil_printf("Target platform is vxWorks\r\n");
    sup.server_port = 1122;
    sup.set_nbio = 0;
#endif

    // Initialize Buffer Pool
    BufPool_Init(&BufferPool);
    sup.bp = &BufferPool;

    // Open the UDP socket server
    if ((status = SockUDP_Open(&su, &sup)) < 0) {
        xil_printf("SockUDP_Open failed: status = %d\r\n", status);
        goto end;
    } else {
        xil_printf("SockUDP_Open done\r\n");
    }

    // Various init
    port_dbg = (volatile unsigned int*) XPAR_XPS_GPIO_0_BASEADDR;
    signal_dbg = 0x0;
    *port_dbg = signal_dbg;

    // Main data server loop
    done = 0;
    tx_buf = 0;
    old_tx_buf = 0;
    while (done == 0) {
        // Get UDP datagrams
        time_out = 0;
        while ((sz = SockUDP_Recv(&su, (void*) &pdata_rx, &size)) == 0) {
            time_out++;
            if (time_out > 100000000) {
                done = 1;
                xil_printf("SockUDP_Recv time out\r\n");
                break;
            }
        }

        // Set debug signal bit 0
        signal_dbg = signal_dbg | 0x1;
        *port_dbg = signal_dbg;

        // Analyze command
        if (sz > 0) {
            // Get arguments from command
            if (sscanf(pdata_rx, "areq %d %d 0 %d %d\n", &req_size, &req_opt, &req_beg, &req_end) == 4) {
                req_cnt = req_end - req_beg + 1;
                for (i = 0; i < req_cnt; i++) {
                    // Toggle debug signal bit 1
                    signal_dbg = signal_dbg ^ 0x2;
                    *port_dbg = signal_dbg;

                    // Get a buffer for response
                    if ((status = BufPool_GiveBuffer(su.bp, (void*) (&tx_buf), AUTO_RETURNED)) < 0) {
                        xil_printf("%d BufPool_GiveBuffer failed\r\n", status);
                        break;
                    }

                    // Prepare response header
                    if (i == (req_cnt - 1)) {
                        hdr = PUT_FRAME_TY_V2(FRAME_TYPE_FEM_DATA, 0);
                    } else {
                        hdr = PUT_FRAME_TY_V2(FRAME_TYPE_FEM_DATA, 0);
                    }
                    // Put response header
                    *(((unsigned short*) tx_buf) + 0) = htons(req_size);
                    *(((unsigned short*) tx_buf) + 1) = htons(hdr);

                    // Optionnally fill transmit buffer with data
                    if (req_opt == 0) {
                        // Buffer to be sent without being filled
                    } else if (req_opt == 1) {
                        // Buffer filled by processor with the value of a local variable
                        for (j = 1; j < (req_size / sizeof(int)); j++) {
                            *(((unsigned int*) tx_buf) + j) = i;
                        }
                    } else if (req_opt == 2) {
                        // Buffer filled by processor with data read from I/O port
                        for (j = 1; j < (req_size / sizeof(int)); j++) {
                            *(((unsigned int*) tx_buf) + j) = *port_dbg;
                        }
                    } else {
                        // Buffer sent to be sent without being filled
                    }

                    // Send transmit buffer
                    sz = req_size;
                    if ((sz = SockUDP_Send(&su, (void*) tx_buf, &sz)) < 0) {
                        xil_printf("SockUDP_Send failed with error code %d\r\n", sz);
                        goto end;
                    }
                    // xil_printf("Buffer 0x%x sent\r\n", tx_buf);

                    // Give back previous buffer (if any)
                    /*
                    if (old_tx_buf)
                    {
                        if ( (status = BufPool_ReturnBuffer(su.bp, old_tx_buf)) < 0)
                        {
                            xil_printf("%d BufPool_GiveBuffer failed\r\n", status);
                            break;
                        }
                    }
                    old_tx_buf = tx_buf;
                    */
                }
            } else {
                xil_printf("Syntax error in command:%s\r\n", pdata_rx);
            }
        } else {
            xil_printf("Warning: SockUDP_Recv returned %d\r\n", sz);
        }

        // Clear debug signal bit 0
        signal_dbg = signal_dbg & (~0x1);
        *port_dbg = signal_dbg;
    }

    // Show statistics
    SockUDP_ShowStat(&su);

    // Close the UDP socket server
    if ((status = SockUDP_Close(&su)) < 0) {
        xil_printf("SockUDP_Close failed: status = %d\r\n", status);
    }

    xil_printf("-- Exiting main() --\r\n");

end:
#ifdef TARGET_SOC_STANDALONE
    XCache_DisableDCache();
    XCache_DisableICache();
#endif
    return (0);
}
