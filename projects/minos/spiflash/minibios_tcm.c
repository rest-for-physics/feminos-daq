/*******************************************************************************
                           Minos / TCM
                           _____________________

 File:        minibios_tcm.c

 Description: Mini BIOS for TCM on Enclustra Mars MX2 module
 
 Implementation of function specific to TCM

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  April 2012: created
  
*******************************************************************************/

#include "minibios.h"
#include "platform_spec.h"
#include "tcm.h"

/*******************************************************************************
 Minibios_IsBiosButtonPressed
*******************************************************************************/
int Minibios_IsBiosButtonPressed()
{
	volatile unsigned int *reg_22;

	// Check button to enter minibios setup or return
	reg_22 = (volatile unsigned int *) (XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR + 0x58);
	if (((*reg_22) & R22_BIOS_B) == R22_BIOS_B)
	{
		return (0);
	}
	else
	{
		return(1);
	}
}
