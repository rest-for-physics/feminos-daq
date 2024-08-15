/*******************************************************************************

                           _____________________

 File:        tcm.h

 Description: Implementation file for Trigger Clock Module


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  April 2012: created

  January 2014: removed card_id from argument list of Tcm_Open() and Tcm_Close()

*******************************************************************************/
#include "tcm.h"
#include "xparameters.h"
#include <stdio.h>

/*******************************************************************************
 Tcm_Open
*******************************************************************************/
int Tcm_Open(Tcm* tcm) {
    int i;

    // Initialize pointer to Feminos registers
    for (i = 0; i < NB_OF_TCM_REGISTERS; i++) {
        tcm->reg[i] = (volatile unsigned int*) (XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR + (4 * i));
    }

    // Set name of this object
    sprintf(&(tcm->name[0]), "Tcm");

    return (0);
}

/*******************************************************************************
 Tcm_Close
*******************************************************************************/
int Tcm_Close(Tcm* tcm) {
    return (0);
}

/*******************************************************************************
 Tcm_ActOnRegister
*******************************************************************************/
int Tcm_ActOnRegister(Tcm* tcm, TcmRgAction op, int reg, unsigned int bitsel, unsigned int* dat) {
    unsigned int reg_data;

    if ((reg < 0) || (reg >= NB_OF_TCM_REGISTERS)) {
        return (-1);
    } else {
        if (op == TCM_REG_READ) {
            *dat = *(tcm->reg[reg]) & bitsel;
        } else if (op == TCM_REG_WRITE) {
            reg_data = *(tcm->reg[reg]);
            reg_data = (reg_data & (~bitsel)) | (*dat & bitsel);
            *(tcm->reg[reg]) = reg_data;
        } else {
            return (-1);
        }
    }
    return (0);
}
