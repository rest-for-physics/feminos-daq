/*******************************************************************************

 File:        femproxy.h

 Description: Definitions of proxy for a Feminos card.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
   December 2011 : created

*******************************************************************************/

#ifndef FEMPROXY_H
#define FEMPROXY_H

#include "platform_spec.h"
#include "sock_util.h"

/*******************************************************************************
 Constants, types and global variables
*******************************************************************************/
#define SOCK_REV_SIZE 200 * 1024
#define MAX_REQ_CREDIT_BYTES 16 * 1024
#define CREDIT_THRESHOLD_FOR_REQ 8 * 1024

typedef struct _FemProxy {
    int fem_id;
    int client;
    struct sockaddr_in target;
    unsigned char* target_adr;

    struct sockaddr_in remote;
    int remote_size;
    int rem_port;

    int req_credit;     // amount of bytes that can be requested at this time
    int pnd_recv;       // requested bytes pending receive
    int is_first_req;   // Will the next data request be the first one
    int last_ack_sent;  // Was the last daq command to finish acquisition sent
    int cmd_posted_cnt; // number of command posted
    int cmd_reply_cnt;  // number of command replies
    int is_cmd_pending; // a command is pending and had no reply yet
    int is_data_frame;  // indicates that the last frame processed was event data

    int daq_posted_cnt;     // number of daq request posted
    int daq_reply_cnt;      // number of daq replies
    int daq_reply_loss_cnt; // number of daq replies presumably lost
    int daq_reply_dupl_cnt; // number of daq replies duplicated
    int cmd_failed;         // number of command that failed

    unsigned char req_seq_nb; // sequence number of the next daq request
    unsigned char exp_rep_nb; // expected sequence number of the daq next response

    unsigned char* buf_in;     // buffer to receive data
    unsigned short buf_in_len; // beffer in length
    unsigned char* buf_to_bp;  // buffer to return to buffer pool
    unsigned char* buf_to_eb;  // buffer to be passed to event builder
} FemProxy;

/*******************************************************************************
 Function prototypes
*******************************************************************************/
void FemProxy_Clear(FemProxy* fem);
int FemProxy_Open(FemProxy* fem, int* loc_ip, int* rem_ip_base, int ix, int rpt);
void FemProxy_Close(FemProxy* fem);
int FemProxy_Receive(FemProxy* fem);
void FemProxy_MsgStatClear(FemProxy* fem);

#endif
