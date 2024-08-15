/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        ethernet_axidma.c

 Description: Implementation of a minimal Ethernet/IP interface using Xilinx
 soft Tri-mode Ethernet MAC + AXI DMA in a Spartan-6 device.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  May 2011: created based on LLTEMAC Virtex 4 implementation

  October 2012: Deprecated SockUDP_ReturnBuffer(). This is now replaced
  by SockUDP_AddBufferToPendingAckStack() which gives the option to return
  the buffer immediately or some time later when
  SockUDP_RemoveBufferFromPendingAckStack() is called.

  October 2012: added function SockUDP_CheckTx() which is made of code
  extracted from SockUDP_SendFrame(). In the previous version, it was only
  found that a buffer had been sent by the hardware after some subsequent
  call to SockUDP_SendFrame(). Consequently, the buffer pending acknowledge
  stack was not accurate.

  November 2012: removed XCACHE_FLUSH_DCACHE_RANGE() which does not seem to
  be needed because the MicroBlaze data cache is configured in write-through

  April 2013: file sockudp_axidma.c renamed to ethernet_axidma.c

*******************************************************************************/

#include "platform_spec.h"
#include "xparameters.h"
#include <stdio.h>
#ifdef ETHERNET_PHY_KSZ9021
#include "ksz9021.h"
#endif // ETHERNET_PHY_KSZ9021
#include "ethernet.h"

/*******************************************************************************
 Ethernet_Open
 Program the MAC and IP address of this interface and enable frame reception
*******************************************************************************/
int Ethernet_Open(Ethernet* et, EthernetParam* etp) {
    int i;
    int status;
    XAxiEthernet_Config cfg;
    XAxiDma_Config* dmacfg;
    XStatus xst;
    XAxiDma_Bd BdTemplate;
    XAxiDma_BdRing* RxRingPtr = (XAxiDma_GetRxRing(&(et->xaxidma)));
    XAxiDma_BdRing* TxRingPtr = (XAxiDma_GetTxRing(&(et->xaxidma)));
    char mac_rb[8];
    XAxiDma_Bd* Bd1Ptr;
    unsigned int ether_opt;

    // Check desired size of MTU
    if (
            (etp->mtu > MAC_MAXIMUM_FRAME_SIZE_GBE) ||
            ((etp->des_speed < 1000) && (etp->mtu > MAC_MAXIMUM_FRAME_SIZE_ETH)) ||
            ((etp->des_speed != 1000) && (etp->des_speed != 100) && (etp->des_speed != 10))) {
        printf("Ethernet_Open: illegal MTU/speed combination MTU: %d bytes Speed: %d Mbps\n", etp->mtu, etp->des_speed);
        return (ERR_LLTEMAC_ILL_PARAM);
    } else {
        et->mtu = etp->mtu;
        et->des_speed = etp->des_speed;
    }

    // Parameters for AXI Ethernet with AXI DMA
    cfg.Avb = XPAR_AXIETHERNET_0_AVB;
    cfg.AxiDevBaseAddress = XPAR_AXIETHERNET_0_CONNECTED_BASEADDR;
    cfg.AxiDevType = XPAR_AXIETHERNET_0_TEMAC_TYPE;
    cfg.AxiDmaRxIntr = 0;
    cfg.AxiDmaTxIntr = 0;
    cfg.AxiFifoIntr = XPAR_AXIETHERNET_0_CONNECTED_FIFO_INTR;
    cfg.BaseAddress = XPAR_AXIETHERNET_0_BASEADDR;
    cfg.DeviceId = XPAR_AXIETHERNET_0_DEVICE_ID;
    cfg.ExtMcast = XPAR_AXIETHERNET_0_MCAST_EXTEND;
    cfg.PhyType = XPAR_AXIETHERNET_0_PHY_TYPE;
    cfg.RxCsum = XPAR_AXIETHERNET_0_RXCSUM;
    cfg.RxVlanStrp = XPAR_AXIETHERNET_0_RXVLAN_STRP;
    cfg.RxVlanTag = XPAR_AXIETHERNET_0_RXVLAN_TAG;
    cfg.RxVlanTran = XPAR_AXIETHERNET_0_RXVLAN_TRAN;
    cfg.Stats = XPAR_AXIETHERNET_0_STATS;
    cfg.TemacIntr = XPAR_AXIETHERNET_0_INTR;
    cfg.TemacType = XPAR_AXIETHERNET_0_TEMAC_TYPE;
    cfg.TxCsum = XPAR_AXIETHERNET_0_TXCSUM;
    cfg.TxVlanStrp = XPAR_AXIETHERNET_0_TXVLAN_STRP;
    cfg.TxVlanTag = XPAR_AXIETHERNET_0_TXVLAN_TAG;
    cfg.TxVlanTran = XPAR_AXIETHERNET_0_RXVLAN_TRAN;

    // Initialize Axi Ethernet
    if ((status = XAxiEthernet_CfgInitialize(&(et->xaxie), &cfg, XPAR_AXIETHERNET_0_BASEADDR)) != XST_SUCCESS) {
        printf("Ethernet_Open: XAxiEthernet_CfgInitialize failed.\n");
        return (-1);
    }

    // Set MAC address
    if ((status = XAxiEthernet_SetMacAddress(&(et->xaxie), &etp->loc_mac[0])) != XST_SUCCESS) {
        printf("Ethernet_Open: XAxiEthernet_SetMacAddress failed.\n");
        return (-1);
    }

    // Read-back MAC address
    XAxiEthernet_GetMacAddress(&(et->xaxie), (void*) &(mac_rb[0]));
    /*
     printf("MAC = %02x.%02x.%02x.%02x.%02x.%02x\n",
            mac_rb[0], mac_rb[1], mac_rb[2], mac_rb[3], mac_rb[4], mac_rb[5]);
    */

    // Save MAC address
    for (i = 0; i < 6; i++) {
        et->loc_mac[i] = etp->loc_mac[i];
    }

    // Save local IP address
    for (i = 0; i < 4; i++) {
        et->loc_ip[i] = etp->loc_ip[i];
    }

    // Save buffer manager address
    et->bp = etp->bp;

    // Clear Statistics
    Ethernet_StatClear(et);

    // Clear the Socket Table
    et->call_tcp_out = 0;
    for (i = 0; i < MAX_NB_OF_SOCKS; i++) {
        Socket_CtrlBlockClear(&et->sock_table[0]);
    }
    et->sock_cnt = 0;

    // Clear buffer owner array
    for (i = 0; i < 4; i++) {
        et->bown[i].buffer_min = 0;
        et->bown[i].buffer_max = 0;
        et->bown[i].buffer_max = 0;
    }
    et->bown_cnt = 0;

    // Register Buffer Manager, its buffer range and call-back function
    if ((status =
                 Ethernet_AddBufferOwnerCallBack(
                         et,
                         (void*) et->bp,
                         (unsigned int) &(et->bp->buf[0][0]),
                         (unsigned int) &(et->bp->buf[POOL_NB_OF_BUFFER - 1][POOL_BUFFER_SIZE - 1]),
                         (void*) &(BufPool_ReturnBuffer))) < 0) {
        printf("Ethernet_Open: Ethernet_AddBufferOwnerCallBack failed.\n");
        return (-1);
    }

    // Configure PHY and perform auto-negotiation
    if ((status = Ethernet_InitPhy(et)) < 0) {
        return (status);
    }

    // Set operating speed
    if ((status = XAxiEthernet_SetOperatingSpeed(&(et->xaxie), et->actual_speed)) != XST_SUCCESS) {
        printf("Ethernet_Open: XAxiEthernet_SetOperatingSpeed failed.\n");
        return (-1);
    }

    // Enable Jumbo frame depending on requested MTU size
    if (et->mtu > MAC_MAXIMUM_FRAME_SIZE_ETH) {
        ether_opt = XAE_TRANSMITTER_ENABLE_OPTION | XAE_RECEIVER_ENABLE_OPTION | XAE_FCS_INSERT_OPTION | XAE_JUMBO_OPTION;
    } else {
        ether_opt = XAE_TRANSMITTER_ENABLE_OPTION | XAE_RECEIVER_ENABLE_OPTION | XAE_FCS_INSERT_OPTION;
    }

    // Set Ethernet options
    if ((status = XAxiEthernet_SetOptions(&(et->xaxie), ether_opt)) != XST_SUCCESS) {
        printf("Ethernet_Open: XAxiEthernet_SetOptions failed.\n");
        return (-1);
    }

    // Lookup DMA configuration
    dmacfg = XAxiDma_LookupConfig(XPAR_ETHERNET_DMA_DEVICE_ID);
    if (dmacfg == NULL) {
        printf("Ethernet_Open: XAxiDma_LookupConfig failed.\n");
        return (-1);
    }

    // Init DMA instance
    if ((status = XAxiDma_CfgInitialize(&(et->xaxidma), dmacfg) != XST_SUCCESS)) {
        printf("Ethernet_Open: XAxiDma_CfgInitialize failed.\n");
        return (-1);
    }

    // Setup RxBD space.
    XAxiDma_BdClear(&BdTemplate);

    // Create the RxBD ring
    if ((xst = XAxiDma_BdRingCreate(RxRingPtr, (u32) & (et->RxBdSpace), (u32) & (et->RxBdSpace), BD_ALIGNMENT, RXBD_CNT)) != XST_SUCCESS) {
        printf("Ethernet_Open: XLlDma_BdRingCreate failed for RxBD space\n");
        return XST_FAILURE;
    }
    // printf("XLlDma_BdRingCreate: OK for RxBD space\n");

    if ((xst = XAxiDma_BdRingClone(RxRingPtr, &BdTemplate)) != XST_SUCCESS) {
        printf("Ethernet_Open: XLlDma_BdRingClone failed for RxBD space\n");
        return XST_FAILURE;
    }
    // printf("XLlDma_BdRingClone: OK for RxBD space\n");

    // Setup TxBD space.
    if ((xst = XAxiDma_BdRingCreate(TxRingPtr, (u32) & (et->TxBdSpace), (u32) & (et->TxBdSpace), BD_ALIGNMENT, TXBD_CNT)) != XST_SUCCESS) {
        printf("SockUDP_Open: XLlDma_BdRingCreate failed for TxBD space\n");
        return XST_FAILURE;
    }
    // printf("XLlDma_BdRingCreate: OK for TxBD space\n");

    if ((xst = XAxiDma_BdRingClone(TxRingPtr, &BdTemplate)) != XST_SUCCESS) {
        printf("SockUDP_Open: XLlDma_BdRingClone failed for TxBD space\n");
        return XST_FAILURE;
    }
    // printf("XLlDma_BdRingClone: OK for TxBD space\n");

    // Setup RxBD and RxFrame space
    for (i = 0; i < RXBD_CNT; i++) {
        XCACHE_INVALIDATE_DCACHE_RANGE(&(et->rx_frame[i][0]), et->mtu);

        // Allocate 1 RxBD
        if ((xst = XAxiDma_BdRingAlloc(RxRingPtr, 1, &Bd1Ptr)) != XST_SUCCESS) {
            printf("XLlDma_BdRingAlloc %d failed\n", i);
            return XST_FAILURE;
        }
        // printf("XLlDma_BdRingAlloc %d: OK\n", i);

        /*
         * Setup the BD. The BD template used in the call to XLlTemac_SgSetSpace()
         * set the "last" field of all RxBDs. Therefore we are not required to
         * issue a XLlDma_Bd_mSetLast(Bd1Ptr) here.
         */
        XAxiDma_BdSetBufAddr(Bd1Ptr, (unsigned int) &(et->rx_frame[i][0]));
        /* old API */
        XAxiDma_BdSetLength(Bd1Ptr, et->mtu);
        /* new API */
        // XAxiDma_BdSetLength(Bd1Ptr, et->mtu, 0x3FFF);
        XAxiDma_BdSetId(Bd1Ptr, i);

        // Enqueue to HW
        if ((xst = XAxiDma_BdRingToHw(RxRingPtr, 1, Bd1Ptr)) != XST_SUCCESS) {
            printf("XLlDma_BdRingToHw %d failed\n", i);
            return XST_FAILURE;
        }
        // printf("XAxiDma_BdRingToHw %d: OK\n", i);
    }

    // Start the RxDMA Ring
    if ((xst = XAxiDma_BdRingStart(RxRingPtr)) != XST_SUCCESS) {
        printf("XAxiDma_BdRingStart failed\n");
        return XST_FAILURE;
    }
    // printf("XAxiDma_BdRingStart : OK for Rx ring\n");

    // Start the TxDMA Ring
    if ((xst = XAxiDma_BdRingStart(TxRingPtr)) != XST_SUCCESS) {
        printf("XAxiDma_BdRingStart failed\n");
        return XST_FAILURE;
    }
    // printf("XLlDma_BdRingStart : OK for Tx ring\n");

    // Start Ethernet
    XAxiEthernet_Start(&(et->xaxie));

    et->CurRxBdPtr = 0;

    return (0);
}

/*******************************************************************************
 Ethernet_ReturnBuffer
 Determine the owner of this buffer from its address and call the return
 function of the owner
 Return : 0 on success
          negative on failure
*******************************************************************************/
int Ethernet_ReturnBuffer(Ethernet* et, unsigned int buf) {
    int i;

    // Loop on all possible owners of this buffer
    for (i = (et->bown_cnt - 1); i >= 0; i--) {
        // Determine the owner of this buffer
        if ((buf >= et->bown[i].buffer_min) && (buf <= et->bown[i].buffer_max)) {
            // printf("Calling return function for buffer 0x%x\n", buf);

            // Call the buffer return function of the owner
            et->bown[i].buffer_return(et->bown[i].owner, buf);
            return (0);
        }
    }
    return (ERR_BOWN_NOT_FOUND);
}

/*******************************************************************************
 Ethernet_CheckTx
 Check the content of TxBdRing to release descriptors that have been sent by
 the hardware and either return the buffers to their owners or add them to
 the stack of buffers pending an acknowledge from the remote partner
 Return : 0 on success
          negative on failure
*******************************************************************************/
int Ethernet_CheckTx(Ethernet* et) {
    int i;
    int num;
    int done;
    int err;
    unsigned int id_field;

    XStatus xst;
    XAxiDma_Bd* Bd1Ptr;
    XAxiDma_Bd* Bd2Ptr;

    XAxiDma_BdRing* TxRingPtr = XAxiDma_GetTxRing(&(et->xaxidma));

    // Free any of the TxBD that have been sent by the hardware
    done = 0;
    while (!done) {
        if ((num = XAxiDma_BdRingFromHw(TxRingPtr, 4, &Bd1Ptr)) == 0) {
            done = 1;
        } else {
            // return each buffer of that chain to pool
            Bd2Ptr = Bd1Ptr;
            for (i = 0; i < num; i++) {
                // Get the Id of the BufDesc which contains control information
                id_field = XAxiDma_BdGetId(Bd2Ptr);
                // printf("Ethernet_CheckTx: TxBD 0x%x Id_field=0x%x\n", (int) Bd1Ptr, id_field);

                if ((id_field & RETURN_BUFFER_WHEN_SENT) == RETURN_BUFFER_WHEN_SENT) {
                    if ((err = Ethernet_ReturnBuffer(et, (unsigned int) (XAxiDma_BdGetBufAddr(Bd2Ptr)))) < 0) {
                        return (err);
                    }
                } else {
                }

                Bd2Ptr = XAxiDma_BdRingNext(TxRingPtr, Bd2Ptr);
            }

            // free Tx descriptors
            if ((xst = XAxiDma_BdRingFree(TxRingPtr, num, Bd1Ptr)) != XST_SUCCESS) {
                printf("Ethernet_CheckTx: failed to free %d TxBD 0x%x\n", num, (int) Bd1Ptr);
                return (-1);
            }
            // printf("Ethernet_CheckTx: free %d TxBD 0x%x\n", num, Bd1Ptr);
            et->tx_eth_fr_cnt++;
        }
    }
    return (0);
}

/*******************************************************************************
 Ethernet_SendFrame
 Send an Ethernet frame composed of several buffers. First all buffers are
 flushed in case they were cached. Then this function gets the required number
 of transmit buffer descriptors, associates a data buffer to each of them and
 passes the list to the TxRing DMA hardware.
 Return : 0 on success
          negative on failure
*******************************************************************************/
int Ethernet_SendFrame(Ethernet* et, SGListItem* sgl, int num_el) {
    int i;
    // int num;
    // int done;
    // int err;

    XStatus xst;
    XAxiDma_Bd* BdPtr[4];
    // XAxiDma_Bd *Bd1Ptr;
    // XAxiDma_Bd *Bd2Ptr;

    XAxiDma_BdRing* TxRingPtr = XAxiDma_GetTxRing(&(et->xaxidma));

    // Allocate and setup the required number of TxBDs
    if ((xst = XAxiDma_BdRingAlloc(TxRingPtr, num_el, &BdPtr[0])) != XST_SUCCESS) {
        printf("Ethernet_SendFrame: failed to allocate %d TxBDs\n", num_el);
        return XST_FAILURE;
    }

    // Flush cache range of each buffer and associate it with a TxBD
    for (i = 0; i < num_el; i++) {
        // XCACHE_FLUSH_DCACHE_RANGE((sgl+i)->buf, (sgl+i)->len);

        // Setup TxBD
        if (i == 0) {
            // If only one buffer in the list we must set SOP and EOP flags
            if (num_el == 1) {
                XAxiDma_BdSetCtrl(BdPtr[i], XAXIDMA_BD_CTRL_TXSOF_MASK | XAXIDMA_BD_CTRL_TXEOF_MASK);
            }
            // When on the first of several buffers we must set SOP
            else {
                XAxiDma_BdSetCtrl(BdPtr[i], XAXIDMA_BD_CTRL_TXSOF_MASK);
            }
        }

        // When we are on the last TxBD
        if (i == (num_el - 1)) {
            // When only one buffer in the chain: already setup SOP and EOP
            if (i == 0) {
                // already done
            }
            // When several TxBD in the chain, put EOP on the last one
            else {
                XAxiDma_BdSetCtrl(BdPtr[i], XAXIDMA_BD_CTRL_TXEOF_MASK);
            }
        }
        // When not on the last TxBD, we need to setup the next one
        else {
            BdPtr[i + 1] = XAxiDma_BdRingNext(TxRingPtr, BdPtr[i]);
        }

        // For any TxBD in the list, we need to setup the buffer address, size and flags
        XAxiDma_BdSetBufAddr(BdPtr[i], (unsigned int) (sgl + i)->buf);
        /* old API */
        XAxiDma_BdSetLength(BdPtr[i], (sgl + i)->len);
        /* new API */
        // XAxiDma_BdSetLength(BdPtr[i], (sgl+i)->len, 0x3FFF);
        XAxiDma_BdSetId(BdPtr[i], (sgl + i)->flags);
        // printf("0x%x %d\n", (int) (sgl+i)->buf, (sgl+i)->len);
    }

    // Enqueue TxBD list to HW
    if ((xst = XAxiDma_BdRingToHw(TxRingPtr, num_el, BdPtr[0])) != XST_SUCCESS) {
        printf("Ethernet_SendFrame: XLlDma_BdRingToHw failed\n");
        return XST_FAILURE;
    }

    // printf("Ethernet_SendFrame: done\n");
    return (0);
}

/*******************************************************************************
 Socket_Recv
 This function is called by the application. It handles internally the frames
 that should be forwarded to the application and returns only to the user the
 content of the datagrams received.
*******************************************************************************/
int Socket_Recv(Ethernet* et, int* from, void** data, short* sz) {
    XAxiDma_BdRing* RxRingPtr = XAxiDma_GetRxRing(&(et->xaxidma));
    int ByteCount;
    EthHdr* eth;
    XStatus xst;
    int tl = 0;
    XAxiDma_Bd* Bd1Ptr;
    unsigned int id;

    // Free the RxBD from the last receive operation
    if (et->CurRxBdPtr) {
        // Get the ID of the buffer to return
        id = XAxiDma_BdGetId(et->CurRxBdPtr);

        // Return the RxBD back to the DMA channel
        if ((xst = XAxiDma_BdRingFree(RxRingPtr, 1, et->CurRxBdPtr)) != XST_SUCCESS) {
            printf("Socket_Recv: XLlDma_BdRingFree failed for RxBd 0x%x\n", (unsigned int) et->CurRxBdPtr);
            return XST_FAILURE;
        }
        et->CurRxBdPtr = 0;

        XCACHE_INVALIDATE_DCACHE_RANGE(&(et->rx_frame[id][0]), et->mtu);

        // Allocate 1 RxBD
        if ((xst = XAxiDma_BdRingAlloc(RxRingPtr, 1, &Bd1Ptr)) != XST_SUCCESS) {
            printf("Socket_Recv: XLlDma_BdRingAlloc failed\n");
            return XST_FAILURE;
        }

        // Setup the BD
        XAxiDma_BdSetBufAddr(Bd1Ptr, (unsigned int) &(et->rx_frame[id][0]));
        /* old API */
        XAxiDma_BdSetLength(Bd1Ptr, et->mtu);
        /* new API */
        // XAxiDma_BdSetLength(Bd1Ptr, et->mtu, 0x3FFF);
        XAxiDma_BdSetCtrl(Bd1Ptr, XAXIDMA_BD_STS_RXSOF_MASK | XAXIDMA_BD_STS_RXEOF_MASK);

        // Enqueue to HW
        if ((xst = XAxiDma_BdRingToHw(RxRingPtr, 1, Bd1Ptr) != XST_SUCCESS)) {
            printf("SockUDP_Recv: : XLlDma_BdRingToHw failed\n");
            return XST_FAILURE;
        }
        // printf("Socket_Recv: recycled RxBD %d\n", id);
    }

    // Check if any RxBD was filled by hardware
    if (XAxiDma_BdRingFromHw(RxRingPtr, 1, &(et->CurRxBdPtr)) == 0) {
        return (0);
    }

    /* old API */
    ByteCount = XAxiDma_BdGetActualLength(et->CurRxBdPtr);
    /* new API */
    // ByteCount = XAxiDma_BdGetActualLength (et->CurRxBdPtr, 0x3FFF);
    // printf("XAxiDma_BdGetActualLength : got Rx buffer 0x%x to process size=%d\n", (unsigned int) (et->CurRxBdPtr), ByteCount);

    et->rx_eth_fr_cnt++;

    eth = (EthHdr*) (XAxiDma_BdGetBufAddr(et->CurRxBdPtr));

    /*
    printf (" eth.mac_to  : %x:%x:%x:%x:%x:%x\n", eth->mac_to[0], eth->mac_to[1], eth->mac_to[2], eth->mac_to[3], eth->mac_to[4], eth->mac_to[5]);
    printf (" et->loc_mac : %x:%x:%x:%x:%x:%x\n", et->loc_mac[0], et->loc_mac[1], et->loc_mac[2], et->loc_mac[3], et->loc_mac[4], et->loc_mac[5]);
    printf (" eth.tl      : %x \n ", ntohs(eth->tl));
    */

    // Check for ARP request both on destination broadcast and local MAC address
    if (
            ((eth->mac_to[0] == 0xFF) &&
             (eth->mac_to[1] == 0xFF) &&
             (eth->mac_to[2] == 0xFF) &&
             (eth->mac_to[3] == 0xFF) &&
             (eth->mac_to[4] == 0xFF) &&
             (eth->mac_to[5] == 0xFF) &&
             (ntohs(eth->tl) == ETH_TYPE_LEN_ARP)) ||
            ((eth->mac_to[0] == et->loc_mac[0]) &&
             (eth->mac_to[1] == et->loc_mac[1]) &&
             (eth->mac_to[2] == et->loc_mac[2]) &&
             (eth->mac_to[3] == et->loc_mac[3]) &&
             (eth->mac_to[4] == et->loc_mac[4]) &&
             (eth->mac_to[5] == et->loc_mac[5]) &&
             (ntohs(eth->tl) == ETH_TYPE_LEN_ARP))) {
        tl = Ethernet_ProcessARP(et, ByteCount);

        // the frame is not returned to the upper layer, but any error is forwarded
        *sz = 0;
        if (tl >= 0) {
            return (0);
        } else {
            return (tl);
        }
    }

    // Process unicast IP frames
    else if ((eth->mac_to[0] == et->loc_mac[0]) &&
             (eth->mac_to[1] == et->loc_mac[1]) &&
             (eth->mac_to[2] == et->loc_mac[2]) &&
             (eth->mac_to[3] == et->loc_mac[3]) &&
             (eth->mac_to[4] == et->loc_mac[4]) &&
             (eth->mac_to[5] == et->loc_mac[5]) &&
             (ntohs(eth->tl) == ETH_TYPE_LEN_IP)) {
        tl = Ethernet_ProcessIP(et, eth, ByteCount, from, data);
    }

    // Drop all other frames
    else {
        // printf("Error: unexpected frame ByteCount=%d\n", ByteCount);
        *sz = 0;
        // ByteCount-= sizeof(EthHdr);

        // Remove frame from the RX FIFO

        tl = 0;

        et->rx_eth_drop++;
    }

    if (tl >= 0) {
        *sz = (short) tl;
    } else {
        *sz = 0;
    }
    return (tl);
}

/*******************************************************************************
 Ethernet_AddBufferOwnerCallBack
 This function sets the callback function to call after completion of the
 actual send done by the hardware. The buffer that was sent can belong to one
 of several possible owners. The owner of the buffer is determined by the
 address of the buffer. Each buffer owner must declare the address range of
 the buffers it manages and pass a pointer to its own buffer return function
*******************************************************************************/
int Ethernet_AddBufferOwnerCallBack(Ethernet* et, void* ow, unsigned int buf_min, unsigned int buf_max, void (*retfun)(void* ow, unsigned int badr)) {
    if (et->bown_cnt < 4) {
        et->bown[et->bown_cnt].owner = ow;
        et->bown[et->bown_cnt].buffer_min = buf_min;
        et->bown[et->bown_cnt].buffer_max = buf_max;
        et->bown[et->bown_cnt].buffer_return = retfun;
        et->bown_cnt++;
    } else {
        return (ERR_BOWN_ARRAY_FULL);
    }
    return (0);
}
