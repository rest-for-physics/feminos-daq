/*******************************************************************************
                           Minos / Feminos - TCM
                           _____________________

 File:        cmdi_i2c.c

 Description: Command interpreter functions to control I2C devices on an
  Enclustra Mars MX2 module.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr

 History:

  October 2012: created by extraction from cmdi.c

*******************************************************************************/

#include "cmdi_i2c.h"
#include "error_codes.h"
#include "i2c.h"
#include <stdio.h>

/*******************************************************************************
 Cmdi_I2CCommands
*******************************************************************************/
int Cmdi_I2CCommands(CmdiContext* c) {
    unsigned int param[10];
    int err = 0;
    char* rep;
    char str[120];

    rep = (char*) (c->burep + CFRAME_ASCII_MSG_OFFSET); // Move to beginning of ASCII string in CFRAME
    err = 0;

    // Command to read Mars Module I2C EEPROM
    if (strncmp(c->cmd + 5, "info", 4) == 0) {
        err = I2C_GetModuleInfo(c->i2c_inst, &str[0]);
        sprintf(rep, "%d %s(%02d) %s\n", err, &(c->fem->name[0]), c->fem->card_id, str);
        c->do_reply = 1;
    }

    // Command to read Mars Module I2C Current Monitor
    else if (strncmp(c->cmd + 5, "power", 5) == 0) {
        err = I2C_GetCurrentMonitor(c->i2c_inst, &str[0]);
        sprintf(rep, "%d %s(%02d) %s\n", err, &(c->fem->name[0]), c->fem->card_id, str);
        c->do_reply = 1;
    }

    // Command to read Mars Module I2C Real Time Clock
    else if (strncmp(c->cmd + 5, "clock", 5) == 0) {
        err = I2C_GetDateTimeTemp(c->i2c_inst, &str[0]);
        sprintf(rep, "%d %s(%02d) %s\n", err, &(c->fem->name[0]), c->fem->card_id, str);
        c->do_reply = 1;
    }

    // Command to read Mars Module I2C Real Time Clock
    else if (strncmp(c->cmd + 5, "set", 3) == 0) {
        if (sscanf(c->cmd + 5, "set %d %d %d %d %d %d",
                   &param[0],
                   &param[1],
                   &param[2],
                   &param[3],
                   &param[4],
                   &param[5]) == 6) {
            err = I2C_SetDateTime(c->i2c_inst, &str[0], param[0], param[1], param[2], param[3], param[4], param[5]);
            sprintf(rep, "%d %s(%02d): %s\n", err, &(c->fem->name[0]), c->fem->card_id, str);
        } else {
            err = ERR_SYNTAX;
            sprintf(rep, "%d %s(%02d) syntax: mars set day month year hour min sec\n", err, &(c->fem->name[0]), c->fem->card_id);
        }

        c->do_reply = 1;
    }

    // Unknown command
    else {
        err = ERR_UNKNOWN_COMMAND;
        c->do_reply = 1;
        sprintf(rep, "%d %s(%02d) Unknown command: %s", ERR_UNKNOWN_COMMAND, &(c->fem->name[0]), c->fem->card_id, c->cmd);
    }

    return (err);
}
