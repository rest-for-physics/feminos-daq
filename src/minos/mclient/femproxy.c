/*******************************************************************************

 File:        femproxy.c

 Description: A proxy for a Feminos card.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
   December 2011 : created

  April 2013: added bind() after opening socket to be able to select a specific
  IP address for the client. This is neded when the host computer is equipped
  with several Ethernet adapters or an adapter with multiple ports connected
  to the same network.

  September 2013: changed the arguments for the call to Frame_Print(). We now
  have to pass the size of the frame (i.e. the UDP datagram size - 2 bytes)
  and the pointer to the frame skips the first two bytes which is set to the
  size of the UDP datagram received.

*******************************************************************************/
#include "femproxy.h"
#include "frame.h"

extern int verbose;

#include <stdio.h>

/*******************************************************************************
 FemProxy_Clear()
*******************************************************************************/
void FemProxy_Clear(FemProxy* fem) {
    fem->client = 0;
    fem->fem_id = -1;
    fem->rem_port = 0;
    fem->remote_size = 0;
    fem->target_adr = (unsigned char*) 0;
    fem->req_credit = MAX_REQ_CREDIT_BYTES;
    fem->pnd_recv = 0;
    fem->is_first_req = 1;
    fem->last_ack_sent = 1;

    fem->cmd_posted_cnt = 0;
    fem->cmd_reply_cnt = 0;
    fem->daq_reply_loss_cnt = 0;
    fem->daq_reply_dupl_cnt = 0;

    fem->req_seq_nb = 0;
    fem->exp_rep_nb = 0;

    fem->is_cmd_pending = 0;
    fem->is_data_frame = 0;
    fem->daq_posted_cnt = 0;
    fem->daq_reply_cnt = 0;
    fem->cmd_failed = 0;

    fem->buf_in = (unsigned char*) 0;
    fem->buf_to_bp = (unsigned char*) 0;
    fem->buf_to_eb = (unsigned char*) 0;
}

/*******************************************************************************
 FemProxy_MsgStatClear()
*******************************************************************************/
void FemProxy_MsgStatClear(FemProxy* fem) {
    fem->cmd_posted_cnt = 0;
    fem->cmd_reply_cnt = 0;
    fem->daq_posted_cnt = 0;
    fem->daq_reply_cnt = 0;
    fem->cmd_failed = 0;
    fem->daq_reply_loss_cnt = 0;
    fem->daq_reply_dupl_cnt = 0;
}

/*******************************************************************************
 FemProxy_Open()
*******************************************************************************/
int FemProxy_Open(FemProxy* fem, int* loc_ip, int* rem_ip_base, int ix, int rpt) {

    int err;
    int nb;
    int rcvsz_req = SOCK_REV_SIZE;
    int rcvsz_done;
    int optlen = sizeof(int);
    struct sockaddr_in src;

    // Create client socket
    if ((fem->client = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        err = socket_get_error();
        printf("FemProxy_Open(%d): socket failed: error %d\n", ix, err);
        return (-1);
    }

    // Set socket in non-blocking mode
    nb = 1;
    if ((err = ioctlsocket(fem->client, FIONBIO, &nb)) != 0) {
        err = socket_get_error();
        printf("FemProxy_Open(%d): ioctlsocket failed: error %d\n", ix, err);
        return (-1);
    }

    // Set receive socket size
    if ((err = setsockopt(fem->client, SOL_SOCKET, SO_RCVBUF, (char*) &rcvsz_req, optlen)) != 0) {
        err = socket_get_error();
        printf("FemProxy_Open(%d): setsockopt failed: error %d\n", ix, err);
        return (-1);
    }

    // Get receive socket size
    if ((err = getsockopt(fem->client, SOL_SOCKET, SO_RCVBUF, (char*) &rcvsz_done, &optlen)) != 0) {
        err = socket_get_error();
        printf("FemProxy_Open(%d): getsockopt failed: error %d\n", ix, err);
        return (-1);
    }

    // Check receive socket size
    if (rcvsz_done < rcvsz_req) {
        printf("FemProxy_Open(%d): Warning: recv buffer size set to %d bytes while %d bytes were requested. Data losses may occur\n", rcvsz_done, rcvsz_req);
    }

    // Bind the socket to the local IP address
    src.sin_family = PF_INET;
    if ((*(loc_ip + 0) == 0) && (*(loc_ip + 1) == 0) && (*(loc_ip + 2) == 0) && (*(loc_ip + 3) == 0)) {
        src.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        src.sin_addr.s_addr = htonl(((*(loc_ip + 0) & 0xFF) << 24) | ((*(loc_ip + 1) & 0xFF) << 16) | ((*(loc_ip + 2) & 0xFF) << 8) | ((*(loc_ip + 3) & 0xFF) << 0));
    }
    src.sin_port = 0;
    if ((err = bind(fem->client, (struct sockaddr*) &src, sizeof(struct sockaddr_in))) != 0) {
        err = socket_get_error();
        printf("FemProxy_Open(%d): bind failed: error %d\n", ix, err);
        return (-1);
    }

    // Init target address
    fem->rem_port = rpt;
    fem->target.sin_family = PF_INET;
    fem->target.sin_port = htons((unsigned short) fem->rem_port);
    fem->target_adr = (unsigned char*) &(fem->target.sin_addr.s_addr);
    fem->target_adr[0] = *(rem_ip_base + 0);
    fem->target_adr[1] = *(rem_ip_base + 1);
    fem->target_adr[2] = *(rem_ip_base + 2);
    fem->target_adr[3] = *(rem_ip_base + 3) + ix;
    fem->remote_size = sizeof(fem->remote);

    fem->fem_id = ix;

    return (0);
}

/*******************************************************************************
 FemProxy_Close()
*******************************************************************************/
void FemProxy_Close(FemProxy* fem) {
    if (fem->client) {
        closesocket(fem->client);
        fem->client = 0;
    }
}

/*******************************************************************************
 FemProxy_ProcessFrame()
*******************************************************************************/
int FemProxy_ProcessFrame(FemProxy* fem) {
    short error_code;
    unsigned short* sw;
    unsigned char rep_nb;

    fem->is_data_frame = 0;
    sw = (unsigned short*) fem->buf_in;
    rep_nb = *sw & 0xFF;

    // Data frames are to be passed to the event builder
    if (Frame_IsDFrame((void*) fem->buf_in)) {
        // Check daq reply index
        if (*sw & 0x0100) {
            // First sequence number is used to synch
            fem->exp_rep_nb = rep_nb;
        } else if (rep_nb != fem->exp_rep_nb) {
            if (rep_nb > fem->exp_rep_nb) {
                // Some replies were lost
                fem->daq_reply_loss_cnt += (rep_nb - fem->exp_rep_nb);
            } else {
                // Some replies were lost and the index wrap around
                fem->daq_reply_loss_cnt += (256 + rep_nb - fem->exp_rep_nb);
            }
        }
        // printf("FemProxy_ProcessFrame: rep_nb=0x%x exp_rep_nb=0x%x\n", rep_nb, fem->exp_rep_nb);
        fem->exp_rep_nb = rep_nb + 1;

        // Write the length of the buffer in the first short word of the frame instead of the sequence number
        *sw = fem->buf_in_len;

        fem->is_data_frame = 1;
        fem->daq_reply_cnt++;
        fem->buf_to_eb = fem->buf_in;
        fem->buf_in = (unsigned char*) 0;
        fem->buf_to_bp = (unsigned char*) 0;
        // printf("FemProxy_ProcessFrame: posted DFrame 0x%x to Event Builder Queue %d\n", fem->buf_to_eb, fem->fem_id);
    }
    // Configuration frames corresponds to commands and are printed here
    else if (Frame_IsCFrame((void*) fem->buf_in, &error_code)) {
        // Write the length of the buffer in the first short word of the frame
        *sw = fem->buf_in_len;

        if (verbose) {
            Frame_Print((void*) stdout, (void*) (fem->buf_in + 2), (int) (fem->buf_in_len - 2), FRAME_PRINT_ASCII);
        }
        fem->cmd_reply_cnt++;
        if (error_code < 0) {
            fem->cmd_failed++;
        }
        fem->is_cmd_pending = 0;
        fem->buf_to_bp = fem->buf_in;
        fem->buf_in = (unsigned char*) 0;
        fem->buf_to_eb = (unsigned char*) 0;
    }
    // Monitoring frames are also printed here
    else {
        // Write the length of the buffer in the first short word of the frame
        *sw = fem->buf_in_len;

        // Update local statistics
        fem->cmd_reply_cnt++;
        fem->is_cmd_pending = 0;

        // For message statistics, we also print the local statisitics
        if (Frame_IsMsgStat((void*) fem->buf_in)) {
            printf("Client TX statistics: cmd_cnt=%d daq_req=%d cmd_failed=%d\n", fem->cmd_posted_cnt, fem->daq_posted_cnt, fem->cmd_failed);
            printf("Client RX statistics: cmd_rep=%d daq_rep=%d daq_rep_lost=%d daq_rep_dupli=%d\n", fem->cmd_reply_cnt, fem->daq_reply_cnt, fem->daq_reply_loss_cnt, fem->daq_reply_dupl_cnt);
            Frame_Print((void*) stdout, (void*) (fem->buf_in + 2), (int) (fem->buf_in_len - 2), FRAME_PRINT_ALL);
        } else {
            if (verbose) {
                Frame_Print((void*) stdout, (void*) (fem->buf_in + 2), (int) (fem->buf_in_len - 2), FRAME_PRINT_ALL);
            }
        }

        // Free the buffer: post it to the buffer manager
        fem->buf_to_bp = fem->buf_in;
        fem->buf_in = (unsigned char*) 0;
        fem->buf_to_eb = (unsigned char*) 0;
    }

    return (0);
}

/*******************************************************************************
 FemProxy_Receive()
*******************************************************************************/
int FemProxy_Receive(FemProxy* fem) {
    int length;
    int err;

    err = 0;

    // Read data pending from the socket of this fem
    length = recvfrom(fem->client, fem->buf_in, 8192, 0, (struct sockaddr*) &(fem->remote), &(fem->remote_size));
    if (length < 0) {
        err = socket_get_error();
        return (err);
    } else {
        fem->buf_in_len = (unsigned short) length;
        err = FemProxy_ProcessFrame(fem);
    }
    return (err);
}
