/*******************************************************************************

                           _____________________

 File:        tcm_cmdi.h

 Description: Header file for command interpreter.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  April 2012: created

  October 2012: changed name of structure from TcmCmdiContext to CmdiContext
  which is identical to the command interpreter used for the Feminos. Although
  this might be confusing for people, this allows sharing between the two cards
  some of the command interpreter code. The field name is used to tell if the
  object is a fem or a tcm.

  February 2014: (version 1.2) added commands dcbal_enc dcbal_dec and inv_tcm_clk

  March 2014: (version 1.3) corrected errors in printout message of commands
  reading register 22.

  March 2014: (version 2.0) migrated development chain from ISE 13.1 on Windows
  XP 32 bit To ISE 14.1 on Windows 8.1 64 bit.

  June 2014: (version 2.1) Firmware changes: behavior of time stamp clear,
  event count clear signals; changed function of TTL_OUT(2); added FEM_MASK.
  Software changes: added "feminos mask" command

  June 2014: (version 2.2) Firmware changes: increased dynamic range of dead
  time and inter event time histograms to 32 bits instead of 18 bits.
  Fixed problem in accumulation of inter event time histogram so that all
  events are counted instead of one every two events. Software changes:
  developped code to set PLL in zero delay mode.

  June 2014: (version 2.3) Firmware changes: added logic to handle multiplicity
  triggers. Changed frequency of FPGA configuration clock.
  Software changes: added commands "mult_trig_ena", "mult_trig_dst",
  "mult_more_than" and "mult_less_than"

  June 2014: (version 2.4) Firmware changes: added logic for bit error rate
  tester. Software: added command "tcm_bert"

*******************************************************************************/
#ifndef CMDI_TCM_H
#define CMDI_TCM_H

#include "bufpool.h"
#include "ethernet.h"
#include "tcm.h"
#include "xiic.h"

////////////////////////////////////////////////
// Manage Major/Minor version numbering manually
#define SERVER_VERSION_MAJOR 2
#define SERVER_VERSION_MINOR 3
extern char server_date[];
extern char server_time[];
////////////////////////////////////////////////

// Defines the offset of the ASCII string in a configuration frame reply
// So far we have: 2 bytes (PFX_START_OF_CFRAME)
//                 2 bytes (signed short error code)
//                 2 bytes (PFX_ASCII_MSG_LEN)
// Total:          6 bytes
#define CFRAME_ASCII_MSG_OFFSET 6

typedef struct _CmdiContext {
    int do_reply;
    int reply_is_cframe;
    short rep_size;
    char* cmd;
    void* frrep; // Base address of a reply message
    void* burep; // Base address of the user payload in the reply message
    BufPool* bp;
    Ethernet* et;
    Tcm* fem; // the variable is called fem but it is a tcm
    XIic* i2c_inst;

    int lst_socket; // Socket from which the last message was received
    int daq_socket; // Socket from which the last daq command was received

    int rx_cmd_cnt;
    int err_cmd_cnt;
    int tx_rep_cnt;
} CmdiContext;

int TcmCmdi_Cmd_Interpret(CmdiContext* c);
void TcmCmdi_Context_Init(CmdiContext* c);

#endif
