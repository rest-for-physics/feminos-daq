/*******************************************************************************
                           Minos / TCM
                           _____________________

 File:        tcmserver.c

 Description:

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  April 2012: created

  July 2013: added code for loading PLL and setting proper signal inversion
  in this case

  June 2014: added automatic clock inversion when a zero-delay PLL is used

*******************************************************************************/

#include "busymeter.h"
#include "minibios.h"
#include "periodmeter.h"
#include "platform_spec.h"
#include "socket_ui.h"
#include "spiflash.h"
#include "tcm_cmdi.h"

#include "interrupt.h"

#include "MarsI2C.h"
#include "i2c.h"

#include <stdio.h>

#define FRAME_FORMAT_VERSION 0

// Global variable
Tcm atcm;
BufPool BufferPool;
Ethernet ether;
CmdiContext ctx;

#ifdef HAS_MINIBIOS
Minibios_Data minibiosdata;
#endif

#include "lmk03200.h"

/*******************************************************************************
 main
*******************************************************************************/
int main(void) {

    int status;
    EthernetParam etp;
    short sz, size = 0;
    int time_out;
    int done;
    unsigned int param;

    printf("--- TCM Server V%d.%d (Compiled %s %s) --- \n",
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
    etp.loc_ip[3] = 32;
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

        atcm.card_id = minibiosdata.card_id;
        atcm.pll = minibiosdata.pll;
        atcm.pll_odel = minibiosdata.pll_odel;
    }
#endif

    printf("Card_ID=%d IP_address=%d.%d.%d.%d\n", atcm.card_id, etp.loc_ip[0], etp.loc_ip[1], etp.loc_ip[2], etp.loc_ip[3]);

    // Initialize command interpreter context
    TcmCmdi_Context_Init(&ctx);

    // Open I2C Device
    if ((status = I2C_Open(&IicInstance, &InterruptSystem)) < 0) {
        printf("I2C_Open failed with status %d\n", status);
        return (-1);
    }
    ctx.i2c_inst = &IicInstance;

    // Initialize the dead-time histogram
    HistoInt_Init(&busy_histogram, 0, (BHISTO_SIZE - 1), 1, &hbusy[0]);

    // Initialize the inter event time histogram
    HistoInt_Init(&evper_histogram, 0, (EHISTO_SIZE - 1), 1, &hevper[0]);

    // Initialize Buffer Pool
    BufPool_Init(&BufferPool);
    etp.bp = &BufferPool;
    ctx.bp = &BufferPool;

    // Open the TCM
    if ((status = Tcm_Open(&atcm)) < 0) {
        printf("Tcm_Open failed: status = %d\n", status);
        goto end;
    } else {
        printf("Tcm_Open done\n");
    }
    ctx.fem = &atcm;

    // Configure external PLL if present
    if (atcm.pll != 0) {
        if ((status = UW_ConfigureLMK03200(&atcm)) < 0) {
            printf("UW_ConfigureLMK03200 failed with status %d\n", status);
            goto end;
        }

        // Fanout signals need to be inverted on the model of TCM that has a PLL
        param = R22_INV_TCM_SIG;
        if ((status = Tcm_ActOnRegister(&atcm, TCM_REG_WRITE, 22, R22_INV_TCM_SIG, &param)) < 0) {
            printf("%d Tcm(%d): Tcm_ActOnRegister failed.\n", status, atcm.card_id);
            goto end;
        }

        // When a zero-delay PLL is used, the clock has also to be inverted
        if (atcm.pll == 2) {
            param = R22_INV_TCM_CLK;
            if ((status = Tcm_ActOnRegister(&atcm, TCM_REG_WRITE, 22, R22_INV_TCM_CLK, &param)) < 0) {
                printf("%d Tcm(%d): Tcm_ActOnRegister failed.\n", status, atcm.card_id);
                goto end;
            }
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

    // Main loop
    done = 0;
    time_out = 0;
    while (done == 0) {
        // Get commands from network
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
            TcmCmdi_Cmd_Interpret(&ctx);

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
    }

end:

    // Show statistics
    Ethernet_StatShow(&ether);

    // Close the Ethernet interface
    if ((status = Ethernet_Close(&ether)) < 0) {
        printf("Ethernet_Close failed: status = %d\n", status);
    }

    printf("-- Exiting main() --\n");

    return (0);
}
