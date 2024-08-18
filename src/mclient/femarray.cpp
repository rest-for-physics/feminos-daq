/*******************************************************************************

 File:        femarray.c

 Description: Implementation of array of proxy of Feminos cards.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
   December 2011 : created

   September 2013: changed call to Frame_Print() to pass the size of the frame
   in the argument list and set the frame pointer to the first short word after
   the 2 byte field that contains the size of the datagram.

   October 2013: changed when the sequence number is cleared
   - Previous scheme: when the last daq request is sent, the sequence number
   is cleared locally and no sequence number is put in the daq command so that
   the sequence number is also cleared at the other end
   - New scheme: on the first daq command, the sequence number becomes 0 after
   it is incremented, and the first daq command does not contain the sequence
   number so that it gets cleared at the other end. Sub-sequend daq requests
   have a sequence number starting from 0x00 and incrementing by one unit until
   wrap around.
   The problem with the initial scheme is that after the last daq request is
   sent (i.e. a request for 0 byte or frame), the remote end does not stop
   sending data immediately, but it is allowed to continue to send data as long
   as it has some credits left. The sequence number of the data frames was not
   in the expected incremental order because the last daq request had ask to
   restart it.

*******************************************************************************/

#include "femarray.h"
#include "bufpool.h"
#include "evbuilder.h"
#include "frame.h"
#include "os_al.h"

#include <cstdio>
#include <ctime>

#include "prometheus.h"
#include "storage.h"

/*******************************************************************************
 FemArray_Clear
*******************************************************************************/
void FemArray_Clear(FemArray* fa) {
    int i;

    fa->id = 0;
    fa->state = 0;

    fa->verbose = 0;
    fa->fem_proxy_set = 0x1; // FEM(0) is active by default

    fa->rem_ip_beg[0] = 0;
    fa->rem_ip_beg[1] = 0;
    fa->rem_ip_beg[2] = 0;
    fa->rem_ip_beg[3] = 0;

    fa->rem_ip_beg[0] = 192;
    fa->rem_ip_beg[1] = 168;
    fa->rem_ip_beg[2] = 10;
    fa->rem_ip_beg[3] = 1;
    fa->rem_port = REMOTE_DST_PORT;
    fa->pending_rep_cnt = 0;
    fa->sem_cur_cmd_done = (void*) nullptr;
    fa->daq_infinite = 0;
    fa->daq_size_left = 0;
    fa->daq_size_rcv = 0;
    fa->daq_size_lst = 0;
    fa->daq_last_time.tv_sec = 0;
    fa->daq_last_time.tv_usec = 0;
    fa->is_list_fr_pnd = 0;
    fa->list_fr_cnt = 0;
    fa->is_first_fr = 0;
    fa->req_threshold = CREDIT_THRESHOLD_FOR_REQ;
    fa->cred_unit = 'B'; // Default credit unit is Bytes
    fa->drop_a_credit = 0;
    fa->delay_a_credit = 0;
    for (i = 0; i < MAX_NUMBER_OF_FEMINOS; i++) {
        FemProxy_Clear(&(fa->fp[i]));
    }

    fa->bp = (void*) nullptr;
    fa->eb = (void*) nullptr;

    fa->pedthr = (FILE*) nullptr;
}

/*******************************************************************************
 FemArray_Open
*******************************************************************************/
int FemArray_Open(FemArray* fa) {

    int i;
    int done;
    unsigned int mask;
    int err;

    // Open socket for each FEM present
    err = 0;
    mask = 0x1;
    done = 0;
    for (i = 0; i < MAX_NUMBER_OF_FEMINOS; i++) {
        if (fa->fem_proxy_set & mask) {
            if ((err = FemProxy_Open(&fa->fp[i], &(fa->loc_ip[0]), &(fa->rem_ip_beg[0]), i, fa->rem_port)) < 0) {
                printf("FemProxy_Open failed for FEM %d error %d\n", i, err);
                return (err);
            }
        }
        mask <<= 1;
    }

    // Create a mutex for exclusive access to shared ressources
    if ((err = Mutex_Create(&fa->snd_mutex)) < 0) {
        printf("FemProxy_Open: Mutex_Create failed %d\n", err);
        return (err);
    }

    // Print setup
    if (fa->verbose) {
        printf("---------------------------------\n");
        mask = 0x1;
        for (i = 0; i < MAX_NUMBER_OF_FEMINOS; i++) {
            if (fa->fem_proxy_set & mask) {
                printf("Remote server %2d  : %d.%d.%d.%d:%d\n",
                       i,
                       fa->fp[i].target_adr[0],
                       fa->fp[i].target_adr[1],
                       fa->fp[i].target_adr[2],
                       fa->fp[i].target_adr[3],
                       fa->fp[i].rem_port);
            }
            mask <<= 1;
        }
        printf("---------------------------------\n");
    }

    return (0);
}

/*******************************************************************************
 FemArray_Close
*******************************************************************************/
void FemArray_Close(FemArray* fa) {
    int i;

    // Close all proxy to FEM
    for (i = 0; i < MAX_NUMBER_OF_FEMINOS; i++) {
        FemProxy_Close(&(fa->fp[i]));
    }
}

/*******************************************************************************
 FemArray_SendCommand
*******************************************************************************/
int FemArray_SendCommand(FemArray* fa, unsigned int fem_beg, unsigned int fem_end, unsigned int fem_pat, char* cmd) {
    unsigned int i;
    int err, err2;
    unsigned int mask;

    err = 0;
    mask = 1 << fem_beg;

    // Get mutex to send over the network
    if ((err = Mutex_Lock(fa->snd_mutex)) < 0) {
        printf("FemArray_SendCommand: Mutex_Lock failed %d\n", err);
        return (err);
    }

    // printf("fem_beg=%d fem_end=%d fem_pat=0x%x\n", fem_beg, fem_end, fem_pat);

    fa->pending_rep_cnt = 0;
    // Loop on fem set
    for (i = fem_beg; i <= fem_end; i++) {
        // Is this fem among the target?
        if (mask & fem_pat) {
            // Check that this fem does not have already a command pending
            if (fa->fp[i].is_cmd_pending == 0) {
                fa->fp[i].cmd_posted_cnt++;
                fa->fp[i].is_cmd_pending = 1;
                fa->pending_rep_cnt++;

                // Post command
                if ((err = sendto(fa->fp[i].client,
                                  cmd,
                                  strlen(cmd),
                                  0,
                                  (struct sockaddr*) &(fa->fp[i].target),
                                  sizeof(struct sockaddr))) == -1) {
                    err = socket_get_error();
                    printf("FemArray_SendCommand: sendto fem(%02d) failed: error %d\n", i, err);
                    break;
                }

                // printf("FemArray_SendCommand: sent to FEM(%02d) %s\n", i, cmd);
            } else {
                err = -1;
                printf("FemArray_SendCommand: fem(%02d) cannot send command when previous one still pending\n", i);
                break;
            }
        }

        // Did we do the last fem?
        if (mask == (unsigned int) (1 << fem_end)) {
            break;
        }
        mask <<= 1;
    }

    // Release network mutex
    if ((err2 = Mutex_Unlock(fa->snd_mutex)) < 0) {
        printf("FemArray_SendCommand: Mutex_Unlock failed %d\n", err2);
        return (err2);
    }

    return (err);
}

/*******************************************************************************
 FemArray_SendDaq
*******************************************************************************/
int FemArray_SendDaq(FemArray* fa, unsigned int fem_beg, unsigned int fem_end, unsigned int fem_pat, char* cmd) {
    unsigned int i;
    int err, err2;
    unsigned int mask;
    __int64 daq_sz;
    char daq_cmd[40];
    int fem_daq_sz;
    struct timeval now;
    struct timezone ltz;
    unsigned int diff;
    __int64 daq_size_rcv;
    __int64 daq_size_lst;
    __int64 daq_size_left;
    double daq_speed;
    __int64 daq_norm;
    char daq_u;

    err = 0;
    mask = 1 << fem_beg;

    // Get argument
#ifdef WIN32
    if (sscanf(cmd, "DAQ %I64d", &daq_sz) != 1)
#else
    if (sscanf(cmd, "DAQ %llu", &daq_sz) != 1)
#endif
    {
        if (fa->daq_infinite == 1) {
            printf("infinite DAQ\n");
        } else {
            // Get the current time
            gettimeofday(&now, &ltz);

            daq_size_rcv = fa->daq_size_rcv;
            daq_size_lst = fa->daq_size_lst;
            daq_size_left = fa->daq_size_left;

            // Compute the time difference since last DAQ command
            if (fa->daq_last_time.tv_sec != 0) {
                if (fa->daq_last_time.tv_usec < now.tv_usec) {
                    diff = ((now.tv_sec - fa->daq_last_time.tv_sec) * 1000000) +
                           (now.tv_usec - fa->daq_last_time.tv_usec);
                } else {
                    diff = ((now.tv_sec - fa->daq_last_time.tv_sec - 1) * 1000000) +
                           (now.tv_usec + 1000000 - fa->daq_last_time.tv_usec);
                }
            }

            // Compute transfer speed from last DAQ command to the current one
            if (diff != 0) {
                daq_speed = ((double) (fa->daq_size_rcv - fa->daq_size_lst)) / diff;
            } else {
                daq_speed = 0.0;
            }

            // Compute the data volume collected
            daq_norm = daq_size_rcv;
            daq_u = ' ';
            if (daq_norm > 10240) {
                daq_norm = daq_size_rcv / 1024;
                daq_u = 'K';
            }
            if (daq_norm > 10240) {
                daq_norm = daq_norm / 1024;
                daq_u = 'M';
            }
            if (daq_norm > 10240) {
                daq_norm = daq_norm / 1024;
                daq_u = 'G';
            }
            if (daq_norm > 10240) {
                daq_norm = daq_norm / 1024;
                daq_u = 'T';
            }

            /*
#ifdef WIN32
            printf("0 DAQ: collected %I64d %cB (%I64d bytes %I64d bytes left) speed: %.2f MB/s\n", daq_norm, daq_u, daq_size_rcv, daq_size_left, daq_speed);
#else
            printf("0 DAQ: collected %llu %cB (%llu bytes %llu bytes left) speed: %.2f MB/s\n", daq_norm, daq_u,
                   daq_size_rcv, daq_size_left, daq_speed);

#endif
             */
            // const auto space_left_gb = feminos_daq_prometheus::GetFreeDiskSpaceGigabytes("/");
            const auto speed_events_per_second = feminos_daq_storage::StorageManager::Instance().GetSpeedEventsPerSecond();
            const auto number_of_events = feminos_daq_storage::StorageManager::Instance().GetNumberOfEntries();

            time_t now_time = time(nullptr);
            tm* now_tm = gmtime(&now_time);
            char time_str[80];
            strftime(time_str, 80, "[%Y-%m-%dT%H:%M:%SZ]", now_tm);

            cout << time_str << " | # Entries: " << number_of_events << " | 🏃 Speed: " << speed_events_per_second << " entries/s (" << daq_speed << " MB/s)" << endl;

            auto& manager = feminos_daq_prometheus::PrometheusManager::Instance();

            manager.SetDaqSpeedMB(daq_speed);
            manager.SetDaqSpeedEvents(speed_events_per_second);

            // Update the new time and size of received data
            fa->daq_last_time = now;
            fa->daq_size_lst = daq_size_rcv;
        }

        return 0;
    }

    // Get mutex to send over the network
    if ((err = Mutex_Lock(fa->snd_mutex)) < 0) {
        printf("FemArray_SendDaq: Mutex_Lock failed %d\n", err);
        return (err);
    }

    if (daq_sz == 0) // stop acquisition
    {
        fa->daq_infinite = 0;
        fa->daq_size_left = 0;
    } else if (daq_sz == -1) // unlimited size
    {
        fa->daq_infinite = 1;
    } else if (daq_sz == -2) // pursue on-going acquisition
    {

    } else if (daq_sz > 0) // specify amount of data to collect
    {
        fa->daq_infinite = 0;
        fa->daq_size_left = daq_sz;
        fa->daq_size_rcv = 0;
    }

    // Loop on fem set
    for (i = fem_beg; i <= fem_end; i++) {
        // Is this fem among the target?
        if (mask & fem_pat) {
            // Check that this fem has some credits
            if (fa->fp[i].req_credit >= fa->req_threshold) {
                // Compute the size of this request
                if (fa->cred_unit == 'B') {
                    if (fa->fp[i].req_credit < fa->daq_size_left) {
                        fem_daq_sz = fa->fp[i].req_credit;
                    } else {
                        if (fa->daq_infinite == 1) {
                            fem_daq_sz = fa->fp[i].req_credit;
                        } else {
                            fem_daq_sz = (int) fa->daq_size_left;
                        }
                    }
                } else {
                    fem_daq_sz = fa->fp[i].req_credit;
                }

                if (fa->daq_size_left > 0) {
                    fa->fp[i].last_ack_sent = 0;
                } else {
                    fem_daq_sz = 0;
                }

                if (
                        (fa->daq_size_left > 0) ||
                        (fa->daq_infinite == 1) ||
                        ((fa->daq_size_left <= 0) && (fa->fp[i].last_ack_sent == 0))) {
                    // Prepare the command
                    // When the amount of data to request from this FEM was 0 the request does not have a sequence number
                    /*
                    if (fem_daq_sz == 0)
                    {
                        sprintf(daq_cmd, "daq 0x%06x %c\n", fem_daq_sz, fa->cred_unit);
                    }
                    else
                    {
                        sprintf(daq_cmd, "daq 0x%06x %c 0x%02x\n", fem_daq_sz, fa->cred_unit, fa->fp[i].req_seq_nb);
                    }
                    */

                    // On the first data request to that FEM we clear the local sequence number
                    // and we omit the sequence number in the command to also clear it at the remote end
                    if (fa->fp[i].is_first_req) {
                        sprintf(daq_cmd, "daq 0x%06x %c\n", fem_daq_sz, fa->cred_unit);
                        fa->fp[i].req_seq_nb = 0xFF; // we put 0xFF so that the first sequence number sent will be 0x00
                        fa->fp[i].is_first_req = 0;
                    } else {
                        sprintf(daq_cmd, "daq 0x%06x %c 0x%02x\n", fem_daq_sz, fa->cred_unit, fa->fp[i].req_seq_nb);
                    }

                    if (fa->drop_a_credit == 1) {
                        // Drop the request frame to inject a fault
                    } else {
                        if (fa->delay_a_credit) {
                            // Delay the send of that credit
                            Sleep(fa->delay_a_credit);
                            fa->delay_a_credit = 0;
                        }
                        // Post the command normally
                        if ((err = sendto(fa->fp[i].client,
                                          daq_cmd,
                                          strlen(daq_cmd),
                                          0,
                                          (struct sockaddr*) &(fa->fp[i].target),
                                          sizeof(struct sockaddr))) == -1) {
                            err = socket_get_error();
                            printf("FemArray_SendDaq: sendto fem(%02d) failed: error %d\n", i, err);
                            break;
                        }
                    }
                    fa->drop_a_credit = 0;

                    fa->fp[i].req_credit -= fem_daq_sz;
                    fa->fp[i].pnd_recv += fem_daq_sz;

                    // When the amount of data to request from this FEM was 0, the next future request will be the first one
                    if (fem_daq_sz == 0) {
                        fa->fp[i].is_first_req = 1;
                    }
                    fa->fp[i].req_seq_nb++;

                    // When the amount of data to request from this FEM was 0, we clear the request sequence number
                    /*
                    if (fem_daq_sz == 0)
                    {
                        fa->fp[i].req_seq_nb = 0;
                    }
                    else
                    {
                        fa->fp[i].req_seq_nb++;
                    }
                    */
                    fa->fp[i].daq_posted_cnt++;

                    if (fa->daq_size_left <= 0) {
                        fa->fp[i].last_ack_sent = 1;
                    }
                    // printf("FemArray_SendDaq: sent to FEM(%02d) %s", i, daq_cmd);
                }
            } else {
                // printf("FemArray_SendDaq: fem(%02d) has no more credits.\n", i);
            }
        }

        // Did we do the last fem?
        if (mask == (unsigned int) (1 << fem_end)) {
            break;
        } else {
            mask <<= 1;
        }
    }

    // Release network mutex
    if ((err2 = Mutex_Unlock(fa->snd_mutex)) < 0) {
        printf("FemArray_SendDaq: Mutex_Unlock failed %d\n", err2);
        return (err2);
    }

    return (err);
}

/*******************************************************************************
 FemArray_EventBuilderIO
*******************************************************************************/
int FemArray_EventBuilderIO(FemArray* fa, unsigned int fem_beg, unsigned int fem_end, unsigned int fem_pat) {
    unsigned int i;
    int err, err2;
    unsigned int mask;
    EventBuilder* eb;

    err = 0;
    mask = 1 << fem_beg;
    eb = (EventBuilder*) fa->eb;

    // Get mutex to event builder queues
    if ((err = Mutex_Lock(eb->q_mutex)) < 0) {
        printf("FemArray_EventBuilderIO: Mutex_Lock failed %d\n", err);
        return (err);
    }

    // Loop on fem set
    for (i = fem_beg; i <= fem_end; i++) {
        // Is this fem among the target?
        if (mask & fem_pat) {
            // Does this FEM has a buffer for the Event Builder?
            if (fa->fp[i].buf_to_eb) {
                // Post the buffer to the event builder
                if ((err = EventBuilder_PutBufferToProcess(eb, fa->fp[i].buf_to_eb, i)) < 0) {
                    printf("FemArray_EventBuilderIO: EventBuilder_PutBufferToProcess failed %d\n", err);
                    return (err);
                }
                fa->fp[i].buf_to_eb = 0;
            }
        }
        // Did we do the last fem?
        if (mask == (unsigned int) (1 << fem_end)) {
            break;
        } else {
            mask <<= 1;
        }
    }

    // Release mutex to event builder queues
    if ((err2 = Mutex_Unlock(eb->q_mutex)) < 0) {
        printf("FemArray_EventBuilderIO: Mutex_Unlock failed %d\n", err2);
        return (err2);
    }

    // Wakeup the event builder
    if ((err = Semaphore_Signal(eb->sem_wakeup)) < 0) {
        printf("FemArray_EventBuilderIO: Semaphore_Signal failed %d\n", err);
        return (err);
    }

    return (err);
}

/*******************************************************************************
 FemArray_SavePedThrList
*******************************************************************************/
int FemArray_SavePedThrList(FemArray* fa, void* buf) {
    struct tm* now;
    char file_str[80];
    time_t start_time;
    int err;
    char namec[8];
    unsigned short sz;
    unsigned short* buf_s;

    err = 0;

    // On the first frame received, we open a new file
    if (fa->is_first_fr == 1) {
        // Get the time of start
        time(&start_time);
        now = localtime(&start_time);

        // See if we are saving pedestals or thresholds
        if (fa->is_list_fr_pnd == 1) {
            sprintf(namec, "ped_");
        } else {
            sprintf(namec, "thr_");
        }

        // Prepare the file name string
        sprintf(file_str, "%s%s%4d_%02d_%02d-%02d_%02d_%02d.txt",
                &(((EventBuilder*) fa->eb)->file_path[0]),
                namec,
                ((now->tm_year) + 1900),
                ((now->tm_mon) + 1),
                now->tm_mday,
                now->tm_hour,
                now->tm_min,
                now->tm_sec);

        printf("Pedestals/Thresholds saved to: %s\n", file_str);

        // Open result file
        if (!(fa->pedthr = fopen(file_str, "w"))) {
            printf("FemArray_SavePedThrList: could not open file %s.\n", file_str);
            fa->pedthr = (FILE*) 0;
            return (-1);
        } else {
            fa->is_first_fr = 0;
        }
    }

    // Save pedestal/thresholds to file
    if (fa->pedthr) {
        // Get the size field and skip it
        buf_s = (unsigned short*) buf;
        sz = *buf_s;
        buf_s++;
        sz -= 2;
        Frame_Print((void*) fa->pedthr, (void*) buf_s, (int) sz, FRAME_PRINT_LISTS);
    }

    if (fa->list_fr_cnt > 0) {
        fa->list_fr_cnt--;
        printf("FemArray_SavePedThrList: expecting %d frames\n", fa->list_fr_cnt);
    } else {
        printf("Warning: FemArray_SavePedThrList received an unexpected frame!\n");
    }

    // Close the result file if this was the last frame that was expected
    if (fa->list_fr_cnt == 0) {
        if (fa->pedthr) {
            fclose(fa->pedthr);
        }
    }

    return (err);
}

/*******************************************************************************
 FemArray_ReceiveLoop
*******************************************************************************/
int FemArray_ReceiveLoop(FemArray* fa) {
    int err;
    struct timeval t_timeout;
    unsigned int mask;
    unsigned int i;
    int nsock;
    int smax;
    fd_set readfds, writefds, exceptfds, readfds_work;
    int no_longer_pnd_cnt;
    int was_pnd;
    int signal_cmd;
    int was_event_data;

    // printf("FemArray_ReceiveLoop: started\n");

    // Build the socket descriptor set from which we want to read
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    mask = 0x00000001;
    nsock = 0;
    smax = 0;
    for (i = 0; i < MAX_NUMBER_OF_FEMINOS; i++) {
        if (fa->fem_proxy_set & mask) {
            FD_SET(fa->fp[i].client, &readfds);
            if (fa->fp[i].client > smax) {
                smax = fa->fp[i].client;
            }
            // printf("fa->fp[i].client=%d smax=%d\n", fa->fp[i].client, smax);
            nsock++;
        }
        mask <<= 1;
    }
    smax++;

    // Main loop receiving frames over the network interface
    err = 0;
    while (fa->state) {
        t_timeout.tv_sec = 5;
        t_timeout.tv_usec = 0;

        // Copy the read fds from what we computed outside the loop
        readfds_work = readfds;

        // Wait for any of these sockets to be ready
        if ((err = select(smax, &readfds_work, &writefds, &exceptfds, &t_timeout)) < 0) {
            printf("FemArray_ReceiveLoop: select failed %d\n", err);
            return (err);
        }

        no_longer_pnd_cnt = 0;
        was_event_data = 0;

        if (err == 0) {
            // printf("nothing received!\n");
        } else {
            // Get the network mutex
            if ((err = Mutex_Lock(fa->snd_mutex)) < 0) {
                printf("FemArray_ReceiveLoop: Mutex_Lock failed %d\n", err);
                return (err);
            }

            // printf("received on %d sockets!\n", err);
            mask = 0x00000001;
            was_pnd = 0;
            for (i = 0; i < MAX_NUMBER_OF_FEMINOS; i++) {
                if (fa->fem_proxy_set & mask) {
                    if (FD_ISSET(fa->fp[i].client, &readfds_work)) {
                        // printf("socket %d has pending data\n", err);
                        //  See if there is a command pending reply for that fem
                        was_pnd = fa->fp[i].is_cmd_pending;

                        // Get a receive buffer for that FEM if we do not already have one
                        if (fa->fp[i].buf_in == (unsigned char*) 0) {
                            if ((err = BufPool_GiveBuffer(fa->bp, (void*) (&(fa->fp[i].buf_in)), AUTO_RETURNED)) < 0) {
                                printf("FemArray_ReceiveLoop: BufPool_GiveBuffer failed\n", err);
                                return (err);
                            }
                        }

                        // Receive the frame for that fem
                        if ((err = FemProxy_Receive(&fa->fp[i])) < 0) {
                            return (err);
                        }

                        // If the response is pedestal or thresholds, save them to file on disk
                        if (fa->is_list_fr_pnd) {
                            if ((err = FemArray_SavePedThrList(fa, fa->fp[i].buf_to_bp)) < 0) {
                                printf("FemArray_ReceiveLoop: FemArray_SavePedThrList failed\n", err);
                                return (err);
                            }
                        }

                        // Return this buffer to buffer manager if it is no longer used
                        if (fa->fp[i].buf_to_bp) {
                            BufPool_ReturnBuffer(fa->bp, (unsigned long) (fa->fp[i].buf_to_bp));
                        }

                        // Check if a command was pending and is no longer pending for that fem
                        if ((was_pnd == 1) && (fa->fp[i].is_cmd_pending == 0)) {
                            no_longer_pnd_cnt++;
                        } else {
                            if (!fa->fp[i].is_data_frame) {
                                printf("FemArray_ReceiveLoop: received monitoring or configuration reply frame from FEM %d but no command was pending.\n",
                                       i);
                            }
                        }
                        was_event_data += fa->fp[i].is_data_frame;
                        // printf("was_pnd = %d no_longer_pnd_cnt=%d\n", was_pnd, no_longer_pnd_cnt);
                    }
                }
                mask <<= 1;
            }

            // Release the network mutex
            if ((err = Mutex_Unlock(fa->snd_mutex)) < 0) {
                printf("FemArray_ReceiveLoop: Mutex_Unlock failed %d\n", err);
                return (err);
            }
        }

        signal_cmd = 0;
        // Update the count of responses still needed to be collected for the currently posted command
        if (no_longer_pnd_cnt) {
            // Get the mutex to safely read/modify the pending reply count
            if ((err = Mutex_Lock(fa->snd_mutex)) < 0) {
                printf("FemArray_ReceiveLoop: Mutex_Lock failed %d\n", err);
                return (err);
            }

            // Update pending reply count
            if (fa->pending_rep_cnt >= no_longer_pnd_cnt) {
                fa->pending_rep_cnt -= no_longer_pnd_cnt;
                // Has it reached zero?
                if (fa->pending_rep_cnt == 0) {
                    signal_cmd = 1;
                    fa->is_list_fr_pnd = 0;
                } else {
                    signal_cmd = 0;
                }
            } else {
                printf("Warning: received more ASCII response frames than expected!\n");
            }

            // Release the mutex to safely read/modify the pending reply count
            if ((err = Mutex_Unlock(fa->snd_mutex)) < 0) {
                printf("FemArray_SendCommand: Mutex_Unlock failed %d\n", err);
                return (err);
            }
        }

        // Signal CmdFetcher if the current command has been completed
        if (signal_cmd) {
            if ((err = Semaphore_Signal(fa->sem_cur_cmd_done)) < 0) {
                printf("FemArray_ReceiveLoop: Semaphore_Signal failed %d\n", err);
                return (err);
            }
        }

        // Perform the required buffer IO with the event builder
        if ((err = FemArray_EventBuilderIO(fa, 0, 31, fa->fem_proxy_set)) < 0) {
            return (err);
        }
    }

    printf("FemArray_ReceiveLoop: completed.\n");

    return (err);
}
