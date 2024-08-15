/*******************************************************************************

                           _____________________

 File:        cmdi_strings.h

 Description: Header file for command interpreter.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  January 2012: created by extraction from cmdi.h

*******************************************************************************/
#ifndef CMDI_STRINGS_H
#define CMDI_STRINGS_H

/* RBF data target interpretation  */
static char Cmdi_RbfTarget2str[4][16] = {
        "drop_data",       // 0
        "remote_DAQ",      // 1
        "local_ped_histo", // 2
        "local_hit_histo"  // 3
};

/* AXI Ring Buffer timed field interpretation  */
static char AxiRingBuffer_Timed2str[2][16] = {
        "infinite_wait", // 0
        "timed_wait"     // 1
};

/* AXI Ring Buffer timeout field interpretation  */
static char AxiRingBuffer_Timeval2str[4][6] = {
        "1ms",   // 0
        "10ms",  // 1
        "100ms", // 2
        "1s"     // 3
};

/* Loss policy data target interpretation  */
static char Cmdi_LossPolicy2str[3][12] = {
        "ignore",    // 0
        "re-credit", // 1
        "re-send"    // 2
};

#endif
