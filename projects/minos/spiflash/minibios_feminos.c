/*******************************************************************************
                           Minos / Feminos
                           _____________________

 File:        minibios_feminos.c

 Description: Mini BIOS for Feminos card on Enclustra Mars MX2 module

 Implementation of function specific to Feminos card

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  April 2012: created

*******************************************************************************/

#include "feminos.h"
#include "minibios.h"
#include "platform_spec.h"

/*******************************************************************************
 Minibios_IsBiosButtonPressed
*******************************************************************************/
int Minibios_IsBiosButtonPressed() {
    volatile unsigned int* reg_0;

    // Check button to enter minibios setup or return
    reg_0 = (volatile unsigned int*) (XPAR_AXI_BRAM_CTRL_4_S_AXI_BASEADDR + 0x00);
    if (((*reg_0) & R0_BIOS_B) == R0_BIOS_B) {
        return (0);
    } else {
        return (1);
    }
}
