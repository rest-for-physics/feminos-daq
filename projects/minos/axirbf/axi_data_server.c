/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        axi_data_server.c

 Description:

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2010: created

  September 2012: changed the way response frames are sent to client programs
  over the Ethernet link. In the previous version, the source IP, MAC address
  and ports of the client were saved at one place when receiving each command.
  Responses were always implicitly sent to the client that had sent the last
  command. It was therefore not possible to run the configuration/monitoring
  tasks and the data acquisition task in different programs, possibly running
  on different hosts. The new scheme allows that the same or different client
  programs running on one or different hosts are transparently used for these
  tasks. Several client programs can be used simultaneously for the monitoring
  and configuration tasks provided that their commands do not interfere.
  On the other hand, only one client program is allowed at a time for DAQ tasks.

  October 2012: added call to SockUDP_CheckTx() in the main loop of tasks.
  This allows keeping up-to-date the list of buffers that have been sent
  by the hardware.

*******************************************************************************/
#include "axi_rbf.h"
#include "busymeter.h"
#include "platform_spec.h"
#include "socket_ui.h"
#include "spiflash.h"
#include <stdio.h>

#include "cmdi.h"
#include "feminos.h"
#include "frame.h"
#include "minibios.h"
#include "pedestal.h"

#include "finetime.h"
#include "hit_histo.h"
#include "hitscurve.h"

#include "interrupt.h"

#include "MarsI2C.h"
#include "i2c.h"

#define FRAME_FORMAT_VERSION 0

// Global variable
#ifdef HAS_PEDESTAL_HISTOGRAMS
phisto pedestal_histogram[MAX_NB_OF_ASIC_PER_FEMINOS][ASIC_MAX_CHAN_INDEX + 1];
#endif

#ifdef HAS_SHISTOGRAMS
FeminosShisto fshisto;
#endif

AxiRingBuffer arbf;

BufPool BufferPool;
Ethernet ether;
CmdiContext ctx;
Feminos feminos;

#ifdef HAS_MINIBIOS
Minibios_Data minibiosdata;
#endif

/*******************************************************************************
 Serve_DataToDaq
*******************************************************************************/
int Serve_DataToDaq(CmdiContext* c) {
    int j;
    int status;
    unsigned int bufdesc;
    unsigned int* data_ptr;
    int data_sz;
    unsigned short* data_ptr_s;

    short sz;

    // Check that we have some credits for sending
    if (c->snd_allowed > 0) {
        // Get filled Buffers
        if ((status = AxiRingBuffer_GetFilled(c->ar, &bufdesc)) < 0) {
            printf("AxiRingBuffer_GetFilled failed\n");
            return (-1);
        }

        // If we have any filled buffer
        if (bufdesc != 0xFFFFFFFF) {
            // printf("Filled Buffer 0x%08x\n", bufdesc);
            data_ptr = (unsigned int*) AXIRBF_GET_BDADDR(c->ar, bufdesc);
            data_sz = AXIRBF_GET_BDSIZE(bufdesc);

            // Put the END_OF_FRAME indicator and update size
            data_ptr_s = (unsigned short*) ((unsigned int) data_ptr + data_sz + AXIRBF_BUFFER_USER_SW_OFFSET);
            *data_ptr_s = PFX_END_OF_FRAME;
            data_sz += 2;

            // The first 4 bytes are filled by software
            data_ptr_s = (unsigned short*) (AXIRBF_GET_BDADDR(c->ar, bufdesc) + AXIRBF_BUFFER_USER_SW_OFFSET);
            *data_ptr_s = PUT_FVERSION_FEMID(PFX_START_OF_DFRAME, FRAME_FORMAT_VERSION, c->fem->card_id);
            data_ptr_s++;
            *data_ptr_s = (unsigned short) data_sz;

            // printf(" adr 0x%08x size=%d bytes\n", (unsigned int) data_ptr, data_sz);
            /*
            for(j=0; j<data_sz; j+=4)
            {
                printf("  data(%d)=0x%08x\n", j, *data_ptr);
                data_ptr++;
            }
             */

            // Size of data sent is actually 2 bytes more because user data has been aligned
            // on next 32-bit boundary after Ethernet/IP/UDP headers
            data_ptr_s = (unsigned short*) (AXIRBF_GET_BDADDR(c->ar, bufdesc) + USER_OFFSET_ETH_IP_UDP_HDR);
            if (c->nxt_rep_isfirst == 1) {
                *data_ptr_s = 0x0100 | c->nxt_rep_ix;
                c->nxt_rep_isfirst = 0;
            } else {
                *data_ptr_s = c->nxt_rep_ix;
            }
            c->nxt_rep_ix++;
            data_sz += (AXIRBF_BUFFER_USER_SW_OFFSET - USER_OFFSET_ETH_IP_UDP_HDR);

            // Note the time when the send was made
            c->last_daq_sent = finetime_get_tick(c->fem->reg[7]);

            // Send the buffer
            sz = (short) data_sz;
            if ((status = Socket_Send(c->et, c->daq_socket, (void*) data_ptr, &sz)) < 0) {
                return (-1);
            }

            c->tx_daq_cnt++;

            // AxiRingBuffer_PostFree(c->ar, data_ptr);

            // Update what we are allowed to send depending on which unit is used for requests
            if (c->request_unit == 'B') {
                c->snd_allowed -= data_sz;
            } else {
                c->snd_allowed--;
            }

            // printf("Serve_Data: sent %d bytes allowed to send %d %c\n", data_sz, c->snd_allowed, c->request_unit);
        }
    }
    return (0);
}

/*******************************************************************************
 Serve_DataToLocal
*******************************************************************************/
int Serve_DataToLocal(CmdiContext* c) {
    int status;
    unsigned int bufdesc;
    unsigned int* data_ptr;
    int data_sz;
    unsigned short* data_ptr_s;

    // Get filled Buffers
    if ((status = AxiRingBuffer_GetFilled(c->ar, &bufdesc)) < 0) {
        printf("AxiRingBuffer_GetFilled failed\n");
        return (-1);
    }

    if (bufdesc != 0xFFFFFFFF) {
        // printf("Filled Buffer 0x%08x\n", bufdesc);
        data_ptr = (unsigned int*) AXIRBF_GET_BDADDR(c->ar, bufdesc);
        data_sz = AXIRBF_GET_BDSIZE(bufdesc);

        // The first 4 bytes are filled by software
        data_ptr_s = (unsigned short*) (AXIRBF_GET_BDADDR(c->ar, bufdesc) + AXIRBF_BUFFER_USER_SW_OFFSET);
        *data_ptr_s = PUT_FVERSION_FEMID(PFX_START_OF_DFRAME, FRAME_FORMAT_VERSION, c->fem->card_id);
        data_ptr_s++;
        *data_ptr_s = (unsigned short) data_sz;

        data_ptr_s = (unsigned short*) (AXIRBF_GET_BDADDR(c->ar, bufdesc) + AXIRBF_BUFFER_USER_SW_OFFSET);

        // Analyze the content of the Frame to find all ADC samples and make histogram of them
        if (ctx.serve_target == RBF_DATA_TARGET_PED_HISTO) {
            status = Pedestal_ScanDFrame(data_ptr_s, data_sz);
        } else if (ctx.serve_target == RBF_DATA_TARGET_HIT_HISTO) {
            // Analyze the content of the frame to build histogram of hit channels
            status = FeminosShisto_ScanDFrame(data_ptr_s, data_sz);
        } else {
            // Should not come here!
            printf("Serve_DataToLocal: Should not come here!\n");
        }

        // Return the buffer
        AxiRingBuffer_PostFree(c->ar, (unsigned int) data_ptr);
    }
    return (status);
}

/*******************************************************************************
 Serve_DataToNull
*******************************************************************************/
int Serve_DataToNull(CmdiContext* c) {
    int status;
    unsigned int bufdesc;
    unsigned int* data_ptr;

    // Get filled Buffers
    if ((status = AxiRingBuffer_GetFilled(c->ar, &bufdesc)) < 0) {
        printf("AxiRingBuffer_GetFilled failed\n");
        return (-1);
    }

    if (bufdesc != 0xFFFFFFFF) {
        // printf("Filled Buffer 0x%08x\r\n", bufdesc);
        data_ptr = (unsigned int*) AXIRBF_GET_BDADDR(c->ar, bufdesc);

        // Return the buffer
        AxiRingBuffer_PostFree(c->ar, (unsigned int) data_ptr);
    }
    return (status);
}

/*******************************************************************************
 PeriodicCheck
*******************************************************************************/
void PeriodicCheck(CmdiContext* c) {
    unsigned int now;
    unsigned int age_last_snd;

    // When a reply frame was sent
    if ((c->last_daq_sent) && (c->request_unit == 'F')) {
        // Compute the time elapsed without any credit being received
        now = finetime_get_tick(c->fem->reg[7]);
        age_last_snd = finetime_diff_tick(c->last_daq_sent, now);

        // See if we have waited too much time before we get credit back
        if (age_last_snd > c->cred_wait_time) {
            c->rx_daq_timeout++;
            c->state = CRED_RETURN_TIMED_OUT;

            // Check if the last time a credit was received is changing
            if (c->last_credit_rcv == c->hist_crcv[3]) {
                printf("No credits returned after %d ms.", 4 * (c->cred_wait_time / CLK_50MHZ_TICK_TO_MS_FACTOR));
                if (c->loss_policy == 1) {
                    printf(" Re-credit abandoned.\n");
                } else if (c->loss_policy == 2) {
                    printf(" Re-send abandoned.\n");
                } else {
                    printf("\n");
                }
                c->last_daq_sent = 0;
            } else {
                // Ignore policy
                if (c->loss_policy == 0) {
                    c->last_daq_sent = 0;
                }
                // Re-credit policy
                else if (c->loss_policy == 1) {
                    c->snd_allowed++;
                    c->last_daq_sent = 0;
                    // printf("Credit return timed-out (%d times). Restoring credit to %d F\n", c->rx_daq_timeout, c->snd_allowed);
                }
                // Re-send policy
                else if (c->loss_policy == 2) {
                    c->snd_allowed++;
                    c->resend_last = 1;
                }
            }

            // Keep track on previous value of when the last credit was received
            c->hist_crcv[3] = c->hist_crcv[2];
            c->hist_crcv[2] = c->hist_crcv[1];
            c->hist_crcv[1] = c->hist_crcv[0];
            c->hist_crcv[0] = c->last_credit_rcv;
        }
    }
}

/*******************************************************************************
 main
*******************************************************************************/
int main(void) {
    int status;
    int i, j;
    EthernetParam etp;
    short sz, size = 0;
    int time_out;
    int done;
    unsigned char feminos_id;
    unsigned int param = 0;

    printf("--- AXI Data Server V%d.%d (Compiled %s %s) --- \n",
           SERVER_VERSION_MAJOR,
           SERVER_VERSION_MINOR,
           server_date,
           server_time);

#ifdef TARGET_MICROBLAZE
    printf("Target platform is Mars MX2 / MicroBlaze\n");
    Xil_ICacheEnable();
    Xil_DCacheEnable();
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
    feminos_id = 0;
#endif

    // Setup the interrupt system;
    if ((status = InterruptSystem_Setup(&InterruptSystem)) < 0) {
        printf("InterruptSystem_Setup failed.\n");
        return (-1);
    }

#ifdef HAS_MINIBIOS
    // Try to open the Spi Flash
    if ((status = SpiFlash_Open(&spiflash, &InterruptSystem)) < 0) {
        printf("SpiFlash_Open failed with status %d\n", status);
        return (-1);
    }

    // Call Minibios
    if ((status = Minibios(&minibiosdata)) < 0) {
        printf("Minibios failed with status %d\n", status);
        return (-1);
    } else {
        etp.mtu = minibiosdata.mtu;
        etp.des_speed = minibiosdata.speed;
        etp.loc_mac[0] = minibiosdata.mac[0];
        etp.loc_mac[1] = minibiosdata.mac[1];
        etp.loc_mac[2] = minibiosdata.mac[2];
        etp.loc_mac[3] = minibiosdata.mac[3];
        etp.loc_mac[4] = minibiosdata.mac[4];
        etp.loc_mac[5] = minibiosdata.mac[5];
        etp.loc_ip[0] = minibiosdata.ip[0];
        etp.loc_ip[1] = minibiosdata.ip[1];
        etp.loc_ip[2] = minibiosdata.ip[2];
        etp.loc_ip[3] = minibiosdata.ip[3];
        feminos_id = minibiosdata.card_id;
    }
#endif

    printf("Card_ID=%d IP_address=%d.%d.%d.%d\n", feminos_id, etp.loc_ip[0], etp.loc_ip[1], etp.loc_ip[2], etp.loc_ip[3]);

    // Initialize command interpreter context
    Cmdi_Context_Init(&ctx);

    // Open I2C Device
    if ((status = I2C_Open(&IicInstance, &InterruptSystem)) < 0) {
        printf("I2C_Open failed with status %d\n", status);
        return (-1);
    }
    ctx.i2c_inst = &IicInstance;

    // Initialize AXI Ring Buffer (set buffer capacity to 8 bytes less than MTU for safety
    if ((status = AxiRingBuffer_Init(&arbf, (minibiosdata.mtu - 8))) < 0) {
        printf("AxiRingBuffer_Init failed with status %d\n", status);
        return (-1);
    }
    ctx.ar = &arbf;

    // Initialize Buffer Pool
    BufPool_Init(&BufferPool);
    etp.bp = &BufferPool;
    ctx.bp = &BufferPool;

    // Initialize the dead-time histogram
    HistoInt_Init(&busy_histogram, 0, (BHISTO_SIZE - 1), 1, &hbusy[0]);

#ifdef HAS_PEDESTAL_HISTOGRAMS
    // Initialize the Pedestal histograms
    for (i = 0; i < MAX_NB_OF_ASIC_PER_FEMINOS; i++) {
        for (j = 0; j <= ASIC_MAX_CHAN_INDEX; j++) {
            Histo_Init(&(pedestal_histogram[i][j].ped_histo), 0, (PHISTO_SIZE - 1), 1, &(pedestal_histogram[i][j].ped_bins[0]));
            pedestal_histogram[i][j].stat_valid = 0;
        }
    }
#endif

#ifdef HAS_SHISTOGRAMS
    FeminosShisto_ClearAll(&fshisto);
#endif

    // Initialize the Channel Hit Count Histograms
    for (i = 0; i < MAX_NB_OF_ASIC_PER_FEMINOS; i++) {
        HistoInt_Init(&(ch_hit_cnt_histo[i]), 0, (ASIC_MAX_CHAN_INDEX + 1), 1, &hhitcnt[i][0]);
    }

    // Open the Feminos card
    if ((status = Feminos_Open(&feminos, feminos_id)) < 0) {
        printf("Feminos_Open failed: status = %d\n", status);
        goto end;
    } else {
        printf("Feminos_Open done\n");
    }
    ctx.fem = &feminos;

    // Power ON the FEC is set so by minibios
    if ((minibiosdata.fecon == 'Y') || (minibiosdata.fecon == 'y')) {
        param = R1_FEC_ENABLE;
        if ((status = Feminos_ActOnRegister(&feminos, FM_REG_WRITE, 1, R1_FEC_ENABLE, &param)) < 0) {
            printf("Feminos_ActOnRegister failed: status = %d\n", status);
        }
    }

    // Open the Ethernet interface
    if ((status = Ethernet_Open(&ether, &etp)) < 0) {
        printf("Ethernet_Open failed: status = %d\n", status);
        goto end;
    } else {
        printf("Ethernet_Open done\n");
    }
    ctx.et = &ether;

    // Open command server on UDP "port any"
    if ((status = Socket_Open(&ether, UDP_PORT_ANY, IP_PROTO_UDP, LISTEN)) < 0) {
        printf("Socket_Open failed: status = %d\n", status);
        goto end;
    }

    // Open telnet server on TCP port 23
    if ((status = Socket_Open(&ether, 23, IP_PROTO_TCP, LISTEN)) < 0) {
        printf("Socket_Open failed: status = %d\n", status);
        goto end;
    }

    // Start Axi Ring Buffer
    AxiRingBuffer_IoCtrl(&arbf, AXIRBF_IOCONTROL_MASK, (AXIRBF_RUN | AXIRBF_TIMED | AXIRBF_TIMEVAL_MASK));

    // Register AXI Ring Buffer, its buffer range and call-back function
    if ((status = Ethernet_AddBufferOwnerCallBack(&ether, (void*) &arbf, arbf.base, arbf.last, (void*) &(AxiRingBuffer_PostFree))) < 0) {
        printf("Ethernet_AddBufferOwnerCallBack failed.\n");
        goto end;
    }

    // Main loop
    done = 0;
    time_out = 0;
    while (done == 0) {
        // Get UDP datagrams
        if ((sz = Socket_Recv(&ether, &(ctx.lst_socket), (void*) &(ctx.cmd), &size)) == 0) {
            time_out++;
            if (time_out > 1000000000) {
                printf("Socket_Recv time out\n");
                time_out = 0;
                // done = 1;
                // break;
            }
        } else if (sz > 0) {
            time_out = 0;
            // terminate string
            *(ctx.cmd + sz) = '\0';

            // Interpret and execute command
            Cmdi_Cmd_Interpret(&ctx);

            // Send response if needed
            if (ctx.do_reply) {
                if ((sz = Socket_Send(&ether, ctx.lst_socket, ctx.frrep, &ctx.rep_size)) < 0) {
                    printf("Socket_Send failed with error code %d\n", sz);
                    goto end;
                }
                ctx.tx_rep_cnt++;
                // The command interpreter will get a new buffer
                ctx.frrep = 0;
            }
        } else {
            printf("Warning: Socket_Recv returned %d\n", sz);
        }

        // Periodically check TX completion
        if ((status = Ethernet_CheckTx(&ether)) < 0) {
            printf("Warning: Ethernet_CheckTx returned %d\n", status);
        }

        // Serve data from AXI ring buffer to DAQ, local use, or drop
        if (ctx.serve_target == RBF_DATA_TARGET_DAQ) {
            if ((status = Serve_DataToDaq(&ctx)) < 0) {
                printf("Warning: Serve_DataToDaq returned %d\n", status);
            }
        } else if ((ctx.serve_target == RBF_DATA_TARGET_PED_HISTO) || (ctx.serve_target == RBF_DATA_TARGET_HIT_HISTO)) {
            if ((status = Serve_DataToLocal(&ctx)) < 0) {
                printf("Warning: Serve_DataToLocal returned %d\n", status);
            }
        } else if (ctx.serve_target == RBF_DATA_TARGET_NULL) {
            if ((status = Serve_DataToNull(&ctx)) < 0) {
                printf("Warning: Serve_DataToNull returned %d\n", status);
            }
        } else {
            printf("Fatal: illegal target for data read form RBF: %d\n", ctx.serve_target);
            goto end;
        }

        // Periodic time checking task
        PeriodicCheck(&ctx);

        // TCP output task
        Ethernet_OutputTCP(&ether);
    }

end:

    // Stop Axi Ring Buffer
    AxiRingBuffer_IoCtrl(&arbf, AXIRBF_IOCONTROL_MASK, AXIRBF_STOP);

    // Close all sockets
    for (i = 0; i < MAX_NB_OF_SOCKS; i++) {
        Socket_Close(&ether, i);
    }

    // Show statistics
    Ethernet_StatShow(&ether);

    // Close the Ethernet interface
    if ((status = Ethernet_Close(&ether)) < 0) {
        printf("Ethernet_Close failed: status = %d\n", status);
    }

    printf("-- Exiting main() --\n");

    return (0);
}
