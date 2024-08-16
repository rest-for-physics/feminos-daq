/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        minibios.c

 Description: Mini BIOS for Feminos/TCM card on Enclustra Mars MX2 module

 Implementation of a minimal "bios" to store/retrieve non-volatile configuation
 parameters for the Feminos/TCM card.
 These parameters are stored in the last page (256 bytes) of the last sector
 (4 KB) of the SPI Flash of Mars MX2 module

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2011: created from T2K version

  December 2012: corrected MINIBIOS_FLASH_SECTOR_BASE

  January 2014: added ifdef to distinguish Feminos and TCM configuration
  Changed PLL yes/no to PLL Type. Added command to set output clock delay
  of PLL.

  June 2014: added fecon member to Minibios_Data structure and command
  'O' to determine the power state of the FEC after Feminos boot.

*******************************************************************************/

#include "minibios.h"
#include "platform_spec.h"
#include "spiflash.h"

#include <stdio.h>
#include <stdlib.h>

extern int read_stdin(char*, int);

#define MINIBIOS_FLASH_PAGE_BASE 0xFFFF00
#define MINIBIOS_FLASH_SECTOR_BASE 0xFFF000

#ifdef TARGET_TCM
static char* minibios_menu = "M/I/T/F/P/D/S/L/Q";
#endif

#ifdef TARGET_FEMINOS
static char* minibios_menu = "M/I/T/F/S/L/Q";
#endif

/*******************************************************************************
 Minibios_SaveToFlash
*******************************************************************************/
int Minibios_SaveToFlash(Minibios_Data* md) {
    int err = -1;

    /* Erase the last sector of the Flash */
    if ((err = SpiFlash_SectorErase(&spiflash, MINIBIOS_FLASH_SECTOR_BASE)) < 0) {
        return (err);
    }

    /* Write our data in the last page of the last sector of the Flash */
    if ((err = SpiFlash_WritePage(&spiflash, 0, MINIBIOS_FLASH_PAGE_BASE, sizeof(Minibios_Data), (unsigned char*) md)) < 0) {
        return (err);
    }

    return (0);
}

/*******************************************************************************
 Minibios_LoadFromFlash
*******************************************************************************/
int Minibios_LoadFromFlash(Minibios_Data* md) {
    unsigned char *c, *d;
    int i;

    int err = -1;

    if ((err = SpiFlash_ReadPage(&spiflash, MINIBIOS_FLASH_PAGE_BASE, sizeof(Minibios_Data), &c)) < 0) {
        return (err);
    }

    d = (unsigned char*) md;
    for (i = 0; i < sizeof(Minibios_Data); i++) {
        *d++ = *c++;
    }

    return (err);
}

/*******************************************************************************
 Minibios_Command
*******************************************************************************/
int Minibios_Command(Minibios_Data* md, char* cmd, char* rep) {
    int i;
    int param[8];
    int err;
    char str[4];
    char ch, ch2;

    err = 0;
    switch (*cmd) {
        /* Quit */
        case 'Q':
            sprintf(rep, "0 Quit Minibios.\n");
            break;

        /* Load from Flash */
        case 'L':
            sprintf(rep, "0 Reload Flash.\n");
            break;

        /* Enter Ethernet MAC address */
        case 'M':
            if (sscanf(&cmd[0], "%c %x:%x:%x:%x:%x:%x",
                       &ch,
                       &param[0],
                       &param[1],
                       &param[2],
                       &param[3],
                       &param[4],
                       &param[5]) == 7) {
                for (i = 0; i < 6; i++) {
                    md->mac[i] = (unsigned char) param[i];
                }
                sprintf(str, "<-");
            } else {
                sprintf(str, "=");
            }
            sprintf(rep, "0 MAC %s %x:%x:%x:%x:%x:%x\n",
                    str,
                    md->mac[0],
                    md->mac[1],
                    md->mac[2],
                    md->mac[3],
                    md->mac[4],
                    md->mac[5]);
            break;

        /* Enter IP address */
        case 'I':
            if (sscanf(&cmd[0], "%c %d.%d.%d.%d",
                       &ch,
                       &param[0],
                       &param[1],
                       &param[2],
                       &param[3]) == 5) {
                for (i = 0; i < 4; i++) {
                    md->ip[i] = (unsigned char) param[i];
                }
                sprintf(str, "<-");
            } else {
                sprintf(str, "=");
            }
            sprintf(rep, "0 IP %s %d.%d.%d.%d\n",
                    str,
                    md->ip[0],
                    md->ip[1],
                    md->ip[2],
                    md->ip[3]);
            break;
            break;

        /* Enter Card ID */
        case 'F':
            if (sscanf(cmd, "%c %d", &ch, &param[0]) == 2) {
                md->card_id = (unsigned char) param[0];
                sprintf(str, "<-");
            } else {
                sprintf(str, "=");
            }
            sprintf(rep, "0 card_id %s %d\n", str, md->card_id);
            break;

        /* Set MTU */
        case 'T':
            if (sscanf(cmd, "%c %d", &ch, &param[0]) == 2) {
                md->mtu = (unsigned short) param[0];
                sprintf(str, "<-");
            } else {
                sprintf(str, "=");
            }
            sprintf(rep, "0 MTU %s %d\n", str, md->mtu);
            break;

        /* Set Speed */
        case 'E':
            if (sscanf(cmd, "%c %d", &ch, &param[0]) == 2) {
                md->speed = (unsigned short) param[0];
                sprintf(str, "<-");
            } else {
                sprintf(str, "=");
            }
            sprintf(rep, "0 Speed %s %d Mbps\n", str, md->speed);
            break;

#ifdef TARGET_TCM
        /* Set PLL */
        case 'P':
            if (sscanf(cmd, "%c %d", &ch, &param[0]) == 2) {
                md->pll = (unsigned char) param[0];
                sprintf(str, "<-");
            } else {
                sprintf(str, "=");
            }
            sprintf(rep, "0 PLL %s %d\n", str, (int) md->pll);
            break;

        /* Set PLL output delay */
        case 'D':
            if (sscanf(cmd, "%c %d", &ch, &param[0]) == 2) {
                md->pll_odel = (unsigned char) param[0];
                sprintf(str, "<-");
            } else {
                sprintf(str, "=");
            }
            sprintf(rep, "0 PLL CLKOUT delay %s %d (%d ps)\n", str, (int) md->pll_odel, (((int) md->pll_odel) * 150));
            break;
#endif

#ifdef TARGET_FEMINOS
        /* Set FEC power state */
        case 'O':
            if (sscanf(cmd, "%c %c", &ch, &ch2) == 2) {
                md->fecon = ch2;
                sprintf(str, "<-");
            } else {
                sprintf(str, "=");
            }
            sprintf(rep, "0 FEC ON %s %c\n", str, md->fecon);
            break;
#endif

        /* Save to Flash */
        case 'S':
            if ((err = Minibios_SaveToFlash(md)) < 0) {
                sprintf(rep, "%d Minibios_Command: Flash write failed.\n", err);
            } else {
                sprintf(rep, "0 Minibios_Command: Flash written.\n");
            }
            break;

        default:
            sprintf(rep, "0 minibios %s\n", minibios_menu);
            break;
    }
    return (err);
}

/*******************************************************************************
 Minibios
*******************************************************************************/
int Minibios(Minibios_Data* md) {
    int done;
    int reload;
    char cmd[40];
    char rep[80];
    int err;
    int getarg;

    // Get settings from Flash
    if ((err = Minibios_LoadFromFlash(md)) < 0) {
        printf("Minibios_LoadFromFlash failed: error %d\n", err);
    }

    // Check button to enter minibios setup or return
    if (Minibios_IsBiosButtonPressed() == 0) {
        return (0);
    }

    // Enter minibios
    printf("--------------\n");
    printf("Mini BIOS MENU\n");
    printf("--------------\n");

    // Main loop
    done = 0;
    reload = 0;
    while (!done) {
        if (reload) {
            // Get settings from EEprom
            if ((err = Minibios_LoadFromFlash(md)) < 0) {
                printf("Minibios_LoadFromFlash failed: error %d\n", err);
            }
            reload = 0;
        }

        // Print Menu
        printf("\f");
        printf("M     MAC Address  : %x:%x:%x:%x:%x:%x\n",
               md->mac[0],
               md->mac[1],
               md->mac[2],
               md->mac[3],
               md->mac[4],
               md->mac[5]);
        printf("I     IP Address   : %d.%d.%d.%d\n",
               md->ip[0],
               md->ip[1],
               md->ip[2],
               md->ip[3]);
        printf("T     Ethernet MTU : %d\n", md->mtu);
        printf("E     Speed        : %d\n", md->speed);
        printf("F     Card ID      : %d\n", md->card_id);
#ifdef TARGET_TCM
        printf("P     TCM PLL type : %d\n", (int) md->pll);
        printf("D     CLKOUT delay : %d\n", (int) md->pll_odel);
#endif
#ifdef TARGET_FEMINOS
        printf("O     FEC power ON : %c\n", md->fecon);
#endif
        printf("S     Save to Flash\n");
        printf("L     Load from Flash\n");
        printf("Q     Quit\n");
        printf("Enter command: ");
        fflush(stdout);

        // Get command
        read_stdin(&cmd[0], 40);
        getarg = 0;
        switch (cmd[0]) {
            case 'Q':
                done = 1;
                break;

            case 'M':
                printf("Enter new MAC Address: ");
                fflush(stdout);
                getarg = 1;
                break;

            case 'I':
                printf("Enter new IP Address: ");
                fflush(stdout);
                getarg = 1;
                break;

            case 'F':
                printf("Enter new Card ID: ");
                fflush(stdout);
                getarg = 1;
                break;

            case 'T':
                printf("Enter new Ethernet MTU: ");
                fflush(stdout);
                getarg = 1;
                break;

            case 'E':
                printf("Enter new Ethernet Speed: ");
                fflush(stdout);
                getarg = 1;
                break;

#ifdef TARGET_TCM
            case 'P':
                printf("Enter PLL type (0:None 1:LMK03000 2:LMK03200): ");
                fflush(stdout);
                getarg = 1;
                break;

            case 'D':
                printf("Enter PLL CLKOUT Delay (0 to 15 units of 150 ps): ");
                fflush(stdout);
                getarg = 1;
                break;
#endif

#ifdef TARGET_FEMINOS
            case 'O':
                printf("Power FEC after boot (y:ON  n:OFF): ");
                fflush(stdout);
                getarg = 1;
                break;
#endif

            case 'S':
                reload = 1;
                break;

            case 'L':
                reload = 1;
                break;

            default:

                break;
        }

        // Get additional argument
        if (getarg) {
            read_stdin(&cmd[2], 40);
        }

        // Execute command
        cmd[1] = ' ';
        Minibios_Command(md, &cmd[0], &rep[0]);
        printf("\n%s", rep);
    }
    return (0);
}
