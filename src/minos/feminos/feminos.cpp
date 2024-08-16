/*******************************************************************************

                           _____________________

 File:        feminos.c

 Description: Implementation file for Feminos board


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  June 2011: created

  June 2014: made initialization of pointers to Feminos registers inside a loop
  rather that one by one

*******************************************************************************/
#include "feminos.h"
#include "xparameters.h"

#include <cstdio>
#include <string.h>

/*******************************************************************************
 Feminos_Open
*******************************************************************************/
int Feminos_Open(Feminos* fem, int id) {
    int i, j, k;
    unsigned int reg_0;

    // Initialize pointer to Feminos registers
    for (i = 0; i < NB_OF_FEMINOS_REGISTERS; i++) {
        fem->reg[i] = (volatile unsigned int*) (XPAR_AXI_BRAM_CTRL_4_S_AXI_BASEADDR + (0x04 * i));
    }

    // Name this object
    sprintf(&(fem->name[0]), "Fem");

    // Save Feminos ID
    fem->card_id = id;

    // Write id in Register 0
    reg_0 = R0_PUT_CARD_IX(0, id);
    *(fem->reg[0]) = reg_0;

    // Clear all ASIC register mirror memory
    for (i = 0; i < MAX_NB_OF_ASIC_PER_FEMINOS; i++) {
        for (j = 0; j < BLEND_ASIC_REGISTER_MAX_NB_OF_REGISTERS; j++) {
            for (k = 0; k < BLEND_ASIC_REGISTER_MAX_SIZE_IN_SHORT; k++) {
                fem->asics[i].bar[j].bars[k] = 0;
            }
        }
    }
    return (0);
}

/*******************************************************************************
 Feminos_Close
*******************************************************************************/
int Feminos_Close(Feminos* fem, int id) {
    return (0);
}

/*******************************************************************************
 Feminos_ActOnRegister
*******************************************************************************/
int Feminos_ActOnRegister(Feminos* fem, FmRgAction op, int reg, unsigned int bitsel, unsigned int* dat) {
    unsigned int reg_data;

    if ((reg < 0) || (reg >= NB_OF_FEMINOS_REGISTERS)) {
        return (-1);
    } else {
        if (op == FM_REG_READ) {
            *dat = *(fem->reg[reg]) & bitsel;
        } else if (op == FM_REG_WRITE) {
            reg_data = *(fem->reg[reg]);
            reg_data = (reg_data & (~bitsel)) | (*dat & bitsel);
            *(fem->reg[reg]) = reg_data;
        } else {
            return (-1);
        }
    }
    return (0);
}

/*******************************************************************************
 fstate2s - Feminos state to string
*******************************************************************************/
void fstate2s(char* s, unsigned int val) {
    sprintf(s, " ");
    if (val & R5_ALIGNED) {
        strcat(s, "Aligned ");
    }
    if (val & R5_SCA_WRITE) {
        strcat(s, "SCA_Write ");
    }
    if (val & R5_SCA_READ) {
        strcat(s, "SCA_Read ");
    }
    if (val & R5_DEV_BUSY) {
        strcat(s, "Dev_Busy ");
    }
    if (val & R5_DEV_READY) {
        strcat(s, "Dev_Ready ");
    }
    if (val & R5_ZBT_FULL) {
        strcat(s, "ZBT_Full ");
    }
    if (val & R5_EVF_FULL) {
        strcat(s, "EVF_Full ");
    }
    if (val & R5_RBF_OFIFO_EMPTY) {
        strcat(s, "RBF_OFIFO_Empty ");
    }
}
