/*******************************************************************************

                           _____________________

 File:        cmdi.h

 Description: Header file for command interpreter.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  May 2011: created

  February 2014: (version 1.2) Changed firmware to reset SCA write pointer
  for each event.

  February 2014: (version 1.3) Changed firmware of pulser to support the use
  of the pulser with/without also generating a trigger

  February 2014: (version 1.4) Changed multiplicity trigger firmware

  March 2014: (version 1.5) Fixed bug in "aget x mode" and "aget x test"
  commands

  March 2014: (version 1.6) added command "aget x indyn_range"

  March 2014: (version 2.0) migrated from ISE 13.1 on Windows XP 32 bit
  to ISE 14.1 on Windows 8.1 64 bit
  updated Ethernet core from 2.01a to 3.01a and AXI DMA from version 3.00a to
  5.00a.

  March 2014: (version 2.1) made POLARITY programmable for each ASIC
  independently. CmdiContext member polarity is non a vector instead of a
  scalar.

  March 2014: (version 2.2) Firmware changes:
  - Zero-suppressor now ignores the 4 last time-bins when comparing to threshold
  - Added TCM_IGNORE to be able to exclude a Feminos from the system
  - Changed dead-time measurement: now includes only the dead-time of the
  Feminos and no longer global system dead-time.

  March 2014: (version 2.3) added command "aget x icsa"

  March 2014: (version 2.4) added command "aget x cur_ra" and "aget x cur_buf"

  June 2014: (version 2.5) changed minibios to add FEC power state after boot
  Added command "tstamp_isset", "tstamp_isset clr", "snd_mult_ena"

  June 2014: (version 2.6) increased the number of status/configuration registers
  from 8 to 10. Remapped time_probe to a different bit and register,
  added commands "mult_limit" and "tcm_bert". The corresponding logic was added
  in the firmware.

  June 2014: (version 2.7) added command "erase_hit_ena" and "erase_hit_thr"
  Changed firmware to support these new functions.

*******************************************************************************/
#ifndef CMDI_H
#define CMDI_H

#include "axi_rbf.h"
#include "bufpool.h"
#include "ethernet.h"
#include "feminos.h"
#include "xiic.h"

////////////////////////////////////////////////
// Manage Major/Minor version numbering manually
#define SERVER_VERSION_MAJOR 2
#define SERVER_VERSION_MINOR 7
extern char server_date[];
extern char server_time[];
////////////////////////////////////////////////

typedef struct _PedThr {
    unsigned short ped;
    unsigned short thr;
} PedThr;

#define ASIC_CHAN_TABLE_SZ 128

#define RBF_DATA_TARGET_NULL 0
#define RBF_DATA_TARGET_DAQ 1
#define RBF_DATA_TARGET_PED_HISTO 2
#define RBF_DATA_TARGET_HIT_HISTO 3

// Defines the offset of the ASCII string in a configuration frame reply
// So far we have: 2 bytes (PFX_START_OF_CFRAME)
//                 2 bytes (signed short error code)
//                 2 bytes (PFX_ASCII_MSG_LEN)
// Total:          6 bytes
#define CFRAME_ASCII_MSG_OFFSET 6

// Timer scaling factor to compute clock ticks from time expressed in ms
#define CLK_50MHZ_TICK_TO_MS_FACTOR (1000000 / 20)

extern PedThr pedthrlut[MAX_NB_OF_ASIC_PER_FEMINOS][ASIC_CHAN_TABLE_SZ];

typedef struct _CmdiContext {
    int do_reply;
    int reply_is_cframe;
    int snd_allowed;
    char request_unit;
    short rep_size;
    char* cmd;
    void* frrep; // Base address of a reply message
    void* burep; // Base address of the user payload in the reply message
    BufPool* bp;
    Ethernet* et;
    AxiRingBuffer* ar;
    Feminos* fem;
    XIic* i2c_inst;

    int lst_socket; // Socket from which the last message was received
    int daq_socket; // Socket from which the last daq command was received

    int mode;
    int polarity[MAX_NB_OF_ASIC_PER_FEMINOS];
    int serve_target;

    int rx_cmd_cnt;        // Number of command received (excluding daq)
    int rx_daq_cnt;        // Number of daq requests received
    int rx_daq_timeout;    // Number of times no daq request was not received within expected time window
    int rx_daq_delayed;    // Number of times the expected daq request came but after excessive delay
    int err_cmd_cnt;       // Number of commands that had syntax or format errors
    int tx_rep_cnt;        // Number of command replies sent (except replies to daq commands)
    int tx_daq_cnt;        // Number of replies sent to daq commands
    int tx_daq_resend_cnt; // Number of times made to re-send replies to daq commands
    int daq_miss_cnt;      // Number of daq requests possibly sent by the remote partner but not received

    int loss_policy;    // Behavior when network frame loss detected
    int cred_wait_time; // Time allowed to wait for credit before loss is assumed

    unsigned int last_daq_sent;   // Time when the last data frame was sent
    unsigned int last_credit_rcv; // Time when the last credit frame was received
    unsigned int hist_crcv[4];    // Time history of credit receive time when time-out
    unsigned int cred_rtt;        // Round-trip time of last data frame sent to last credit returned

    unsigned char state;

    unsigned char exp_req_ix;      // Expected index of the next daq request
    unsigned char nxt_rep_ix;      // Index to put in the next daq reply
    unsigned char nxt_rep_isfirst; // Next daq reply is the first in that row

    unsigned int daq_buf_pack;   // Last daq buffer sent. Can be re-sent or needs acknowledge to be released
    unsigned int daq_bufsz_pack; // Size of last buffer sent
    int resend_last;             // Transcient variable to request the last buffer to be re-sent
} CmdiContext;

#define READY_ACCEPT_CREDIT 0
#define CRED_RETURN_TIMED_OUT 1

int Cmdi_Cmd_Interpret(CmdiContext* c);
void Cmdi_Context_Init(CmdiContext* c);

#endif
