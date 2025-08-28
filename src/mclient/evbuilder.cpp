/*******************************************************************************

 File:        evbuilder.c

 Description: Implementation of EventBuilder object.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
   January 2012 : created

   September 2013: changed calls to Frame_Print to pass
frame size in the list of arguments and change the pointer
to the frame to skip the first two bytes of the frame which
contains the size of the datagram

   January 2014: added EventBuilder_Flush() to cleanup any
buffer left from a previous run when starting a new one.
   Added EventBuilder_CheckBuffer() to verify event number
and timestamps depending on event builder mode

*******************************************************************************/

#include "evbuilder.h"
#include "bufpool.h"
#include "femarray.h"
#include "frame.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <thread>

#include "prometheus.h"
#include "storage.h"

extern daqInfo* ShMem_DaqInfo;
extern short int* ShMem_Buffer;

/*
extern int *ShMem_dataReady;
extern int *ShMem_nSignals;
extern unsigned int *ShMem_evId;
extern double *ShMem_timeStamp;
*/

extern int SemaphoreId;

extern int
        sharedBuffer; // it must be set to 1 (in mclientUDP.c)
// if we want to share the resource
extern int
        readOnly; // it must be set to 1 (in mclientUDP.c) if
// we do not want to write data to disk
extern int tcm; // it must be set to 1 (in mclientUDP.c) if
// we are using a TCM

extern int runNumber;
extern int eLogActive;
extern char driftFieldStr[64];
extern char meshVoltageStr[64];
extern char detectorPressureStr[64];
extern char runTagStr[65];
extern char clockStr[16];
extern char shapingStr[16];
extern char gainStr[16];
extern char detectorStr[64];
extern char runComments[512];

int timeStart = 0;

char elogCommand[512];

char command[256];

char fileNameNow[256];
char fileNameEndRun[256];

#if defined(__GNU_LIBRARY__) && \
        !defined(_SEM_SEMUN_UNDEFINED)
// The union is already defined in sys/sem.h
#else
// We must define the union
union semun {
    int val;
    struct semid_ds* buf;
    unsigned short int* array;
    struct seminfo* __buf;
};
#endif

struct sembuf op;

void SemaphoreRed(int id) {
    int sem_id = 0;

    op.sem_num = sem_id; // sem_id
    op.sem_op = -1;
    op.sem_flg = 0;

    semop(id, &op, 1);
}

void SemaphoreGreen(int id) {
    int sem_id = 0;

    op.sem_num = sem_id; // sem_id
    op.sem_op = 1;
    op.sem_flg = 0;

    semop(id, &op, 1);
}

/*******************************************************************************
 EventBuilder_Clear
*******************************************************************************/
void EventBuilder_Clear(EventBuilder* eb) {
    int i, j;

    eb->id = 0;
    eb->state = 0;

    // Clear input Queues
    for (i = 0; i < MAX_NB_OF_SOURCES; i++) {
        for (j = 0; j < MAX_QUEUE_SIZE; j++) {
            eb->q_buf_i[i][j] = (void*) nullptr;
        }
        eb->q_buf_i_rd[i] = 0;
        eb->q_buf_i_wr[i] = 0;
        eb->q_buf_i_sz[i] = 0;
    }

    // Clear output Queue
    for (i = 0; i < MAX_QUEUE_SIZE; i++) {
        eb->q_buf_o[i] = (void*) nullptr;
        eb->q_src_o[i] = 0;
    }
    eb->q_buf_o_rd = 0;
    eb->q_buf_o_wr = 0;
    eb->q_buf_o_sz = 0;

    eb->vflags = 0;

    eb->savedata = 0;
    eb->fout = (FILE*) nullptr;

    eb->byte_wr = 0;
    eb->file_max_size =
            1024 * 1024 *
            1024; // Allow 1 GByte per file by default

    sprintf(&(eb->file_path[0]), "");

    eb->eb_mode = 0;
    eb->pnd_src = 0;
    eb->had_sobe = 0;

    eb->src_had_soe = 0;
    eb->cur_ev_ty = 0;
    eb->cur_ev_nb = 0;
    eb->cur_ev_tsl = 0;
    eb->cur_ev_tsm = 0;
    eb->cur_ev_tsh = 0;

    sprintf(&(eb->run_str[0]), "R???");
    eb->subrun_ix = 0;
}

/*******************************************************************************
 EventBuilder_Open
*******************************************************************************/
int EventBuilder_Open(EventBuilder* eb) {
    int err = 0;

    // Create semaphore to wakeup event builder when new
    // packets are posted
    if ((err = Semaphore_Create(&(eb->sem_wakeup))) < 0) {
        printf(
                "EventBuilder_Open: Semaphore_Create failed "
                "%d\n",
                err);
        return (err);
    }

    // Create a mutex for exclusive access to shared resources
    if ((err = Mutex_Create(&eb->q_mutex)) < 0) {
        printf(
                "EventBuilder_Open: Mutex_Create failed %d\n",
                err);
        return (err);
    }

    return (err);
}

/*******************************************************************************
 EventBuilder_Close
*******************************************************************************/
void EventBuilder_Close(EventBuilder* eb) {
    int err = 0;

    // Delete semaphore
    if (eb->sem_wakeup) {
        if ((err = Semaphore_Delete(&(eb->sem_wakeup))) <
            0) {
            printf(
                    "EventBuilder_Close: Semaphore_Delete "
                    "failed %d\n",
                    err);
        }
    }

    // Delete mutex
    if (eb->q_mutex) {
        if ((err = Mutex_Delete(&eb->q_mutex)) < 0) {
            printf(
                    "EventBuilder_Close: Mutex_Delete failed "
                    "%d\n",
                    err);
        }
    }
}

/*******************************************************************************
 EventBuilder_Flush
*******************************************************************************/
int EventBuilder_Flush(EventBuilder* eb) {
    int src;
    void* buf;
    FemArray* fa;
    int err = 0;

    fa = (FemArray*) eb->fa;

    // Get exclusive access to event builder queues
    if ((err = Mutex_Lock(eb->q_mutex)) < 0) {
        printf("EventBuilder_Flush: Mutex_Lock failed %d\n",
               err);
        return (err);
    }

    // Flush input Queues
    for (src = 0; src < MAX_NB_OF_SOURCES; src++) {
        while (eb->q_buf_i_rd[src] != eb->q_buf_i_wr[src]) {
            // Get the buffer for the input queue of the
            // current source
            buf = eb->q_buf_i[src][eb->q_buf_i_rd[src]];
            // printf("EventBuilder_Flush: dropping buffer
            // 0x%x Source:%d i_rd=%d i_wr=%d i_sz=%d\n",
            // buf, src, eb->q_buf_i_rd[src],
            // eb->q_buf_i_wr[src], eb->q_buf_i_sz[src]);
            eb->q_buf_i_rd[src] =
                    (eb->q_buf_i_rd[src] + 1) % MAX_QUEUE_SIZE;
            eb->q_buf_i_sz[src] = eb->q_buf_i_sz[src] - 1;

            // Return the buffer to the pool
            BufPool_ReturnBuffer(fa->bp,
                                 (unsigned long) buf);
        }
    }

    eb->had_sobe = 0;
    eb->pnd_src = 0;
    // Next event does not have any Start of Event received
    // yet
    eb->src_had_soe = 0;

    // Release mutex to send over the network
    if ((err = Mutex_Unlock(eb->q_mutex)) < 0) {
        printf(
                "EventBuilder_Flush: Mutex_Unlock failed %d\n",
                err);
        return (err);
    }

    return (0);
}

/*******************************************************************************
 EventBuilder_CheckBuffer
*******************************************************************************/
int EventBuilder_CheckBuffer(EventBuilder* eb, int src,
                             void* bu) {
    void* fr;
    unsigned short ev_ty;
    unsigned int ev_nb;
    unsigned short ev_tsl;
    unsigned short ev_tsm;
    unsigned short ev_tsh;
    unsigned int ev_tsml;
    unsigned int eb_ev_tsml;
    unsigned int match;
    int err = 0;

    // See if we want to check event numbers and/or timestamps
    if (eb->eb_mode & 0xE) {
        // Check if this buffer if the first one for this
        // event for this source
        if (!((eb->src_had_soe) & (1 << src))) {
            // Skip size in buffer, start of frame and size
            fr = (void*) ((unsigned int) bu + 6);

            // Get the event type, number and timestamp
            if ((err = Frame_GetEventTyNbTs(
                         fr, &ev_ty, &ev_nb, &ev_tsl, &ev_tsm,
                         &ev_tsh)) < 0) {
                printf(
                        "EventBuilder_CheckBuffer: "
                        "Frame_GetEventTyNbTs failed %d\n",
                        err);
                return (err);
            }

            // Is this the first source for this event?
            if (eb->src_had_soe == 0) {
                // Take the event type, number and timestamp
                // as a reference
                eb->cur_ev_ty = ev_ty;
                eb->cur_ev_nb = ev_nb;
                eb->cur_ev_tsh = ev_tsh;
                eb->cur_ev_tsm = ev_tsm;
                eb->cur_ev_tsl = ev_tsl;
            } else {
                match = 1;

                // See if we want to check event numbers
                if (eb->eb_mode & 0x2) {
                    if ((eb->cur_ev_ty != ev_ty) ||
                        (eb->cur_ev_nb != ev_nb)) {
                        match = 0;
                    }
                }

                // See if we want to check timestamps
                // exactly
                if (eb->eb_mode & 0x4) {
                    if ((eb->cur_ev_tsh != ev_tsh) ||
                        (eb->cur_ev_tsm != ev_tsm) ||
                        (eb->cur_ev_tsl != ev_tsl)) {
                        match = 0;
                    }
                }

                // See if we want to check timestamps +-1
                if (eb->eb_mode & 0x8) {
                    // We assume the 16 MSBs of the
                    // timestamp won't change by adding +-1
                    // to the LSB's
                    ev_tsml =
                            (((unsigned int) ev_tsm) << 16) |
                            ((unsigned int) ev_tsl);
                    eb_ev_tsml =
                            (((unsigned int) eb->cur_ev_tsm)
                             << 16) |
                            ((unsigned int) eb->cur_ev_tsl);

                    if ((eb->cur_ev_tsh != ev_tsh) &&
                        (ev_tsml != 0x00000000) &&
                        (ev_tsml != 0xFFFFFFFF)) {
                        match = 0;
                    }

                    if ((eb_ev_tsml != ev_tsml) &&
                        (eb_ev_tsml != (ev_tsml + 1)) &&
                        (eb_ev_tsml != (ev_tsml - 1))) {
                        match = 0;
                    }
                }

                // Mismatch found
                if (match == 0) {
                    printf(
                            "EventBuilder_CheckBuffer: "
                            "Mismatch Src %02d Event_Type 0x%x "
                            " Event_Count 0x%08x  Time 0x%04x "
                            "0x%04x 0x%04x\n",
                            src, ev_ty, ev_nb, ev_tsh, ev_tsm,
                            ev_tsl);
                    printf(
                            "                                "
                            "Expected: Event_Type 0x%x  "
                            "Event_Count 0x%08x  Time 0x%04x "
                            "0x%04x 0x%04x\n",
                            eb->cur_ev_ty, eb->cur_ev_nb,
                            eb->cur_ev_tsh, eb->cur_ev_tsm,
                            eb->cur_ev_tsl);
                }
            }

            // Remember we got the Start of Event from that
            // source
            eb->src_had_soe |= (1 << src);
        }
    }
    return (err);
}

int EventBuilder_ProcessBuffer(EventBuilder* eb, void* bu) {
    int err = 0;
    unsigned short* bu_s;
    unsigned short sz;
    int wrb;

    // Get frame size from first two bytes of buffer
    bu_s = (unsigned short*) bu;
    sz = *bu_s;

    // skip the field that contains buffer size
    bu_s++;
    sz -= 2;

    // Print data with the desired amount of details
    if (eb->vflags) {
        Frame_Print((void*) stdout, (void*) bu_s, (int) sz,
                    eb->vflags);
    }

    if (sharedBuffer) {
        SemaphoreRed(SemaphoreId);

        Frame_ToSharedMemory((void*) stdout, (void*) bu_s,
                             (int) sz, 0x0, ShMem_DaqInfo,
                             ShMem_Buffer, timeStart, tcm);

        SemaphoreGreen(SemaphoreId);
    }

    // Save data to file
    if (!readOnly && eb->savedata && !feminos_daq_storage::StorageManager::Instance().disable_aqs) {
        // Should we close the current file and open a new one?
        if ((eb->byte_wr + sz) > eb->file_max_size) {
            if ((err = EventBuilder_FileAction(
                         eb, EBFA_CloseCurrentOpenNext,
                         eb->savedata)) < 0) {
                return (err);
            }
        }

        // save in ASCII format
        if (eb->savedata == 1) {
            Frame_Print((void*) eb->fout, bu_s, (int) sz,
                        FRAME_PRINT_ALL);
        }
        // save in binary format
        else if (eb->savedata == 2) {
            // write to file
            if ((wrb = fwrite(bu_s, sz, 1, eb->fout)) ==
                0) {
                printf(
                        "EventBuilder_ProcessBuffer: failed to "
                        "write %d bytes to file\n",
                        sz);
                return (-1);
            }
        }

        eb->byte_wr += sz;
    }

    auto& storage_manager = feminos_daq_storage::StorageManager::Instance();

    if (storage_manager.IsInitialized()) {
        // ReadFrame((void*) bu_s, (int) sz, storage_manager.event);

        // Insert frame
        std::vector<unsigned short> data;
        data.reserve(sz); // Reserve space to avoid reallocations
        std::copy(bu_s, bu_s + sz, std::back_inserter(data));

        storage_manager.AddFrame(data);
    }

    return (err);
}

/*******************************************************************************
 EventBuilder_EmitEventBoundary
*******************************************************************************/
int EventBuilder_EmitEventBoundary(EventBuilder* eb,
                                   int bnd) {
    int err = 0;
    unsigned short buf[16];
    unsigned short sz;
    int wrb;

    if (bnd == 0) {
        buf[0] = 4; // size in bytes
        buf[1] = PFX_START_OF_BUILT_EVENT;
        // cout << "EventBuilder_EmitEventBoundary: start of built event" << endl;
        sz = 4;
    } else {
        buf[0] = 4; // size in bytes
        buf[1] = PFX_END_OF_BUILT_EVENT;
        // cout << "EventBuilder_EmitEventBoundary: end of built event" << endl;
        sz = 4;

        SemaphoreRed(SemaphoreId);
        if (tcm && ShMem_DaqInfo->dataReady == 1) {
            ShMem_DaqInfo->dataReady = 2;
        }
        SemaphoreGreen(SemaphoreId);
    }

    // Print data with the desired amount of details
    if (eb->vflags) {
        Frame_Print((void*) stdout, &buf[1], (int) (sz - 2),
                    eb->vflags);
    }

    // Save data to file
    if (eb->savedata) {
        // save in ASCII format
        if (eb->savedata == 1) {
            Frame_Print((void*) eb->fout, &buf[1],
                        (int) (sz - 2), FRAME_PRINT_ALL);
        }
        // save in binary format
        else if (eb->savedata == 2) {
            // skip the field that contains buffer size
            sz -= 2;

            // write to file
            if ((wrb = fwrite(&buf[1], sz, 1, eb->fout)) ==
                0) {
                printf(
                        "EventBuilder_ProcessBuffer: failed to "
                        "write %d bytes to file\n",
                        sz);
                return (-1);
            } else {
                eb->byte_wr += 2;
            }
        }
        // printf("EventBuilder_EmitEventBoundary: wrote %d
        // bytes to file\n", sz);
    } else {
        // printf("EventBuilder_EmitEventBoundary:
        // unsaved\n");
    }

    return (err);
}

/*******************************************************************************
 EventBuilder_Loop
*******************************************************************************/
int EventBuilder_Loop(EventBuilder* eb) {
    int err = 0;
    void* buf;
    FemArray* fa;
    int done;
    int fem_src;
    unsigned short* sz;
    int len;
    char cmd[40];
    int had_buf;
    int src;
    unsigned int mask;
    int done2;

    // printf("EventBuilder_Loop: started.\n");

    fa = (FemArray*) eb->fa;

    // Event Builder Loop
    while (eb->state) {
        // Wait for new buffer to be posted to event builder
        // if ((err = Semaphore_Wait_Timeout(eb->sem_wakeup,
        // 4000000)) < 0)
        if ((err = Semaphore_Wait(eb->sem_wakeup)) < 0) {
            if (err == -2) {
                // printf("EventBuilder_Loop:
                // Semaphore_Wait_Timeout: timeout
                // detected.\n", err);
            } else {
                printf(
                        "EventBuilder_Loop: "
                        "Semaphore_Wait_Timeout failed %d\n",
                        err);
                return (err);
            }
        }
        // printf("EventBuilder_Loop: new loop\n");

        // Get exclusive access to event builder queues
        if ((err = Mutex_Lock(eb->q_mutex)) < 0) {
            printf(
                    "EventBuilder_Loop: Mutex_Lock failed %d\n",
                    err);
            return (err);
        }

        // If we are building a new event, we expect one
        // event from each active FEM
        if ((eb->pnd_src == 0) && (eb->eb_mode & 0x1)) {
            eb->pnd_src = fa->fem_proxy_set;
        }

        // Process the buffers found in the input queues
        had_buf = 0;
        mask = 0x00000001;
        for (src = 0; src < MAX_NB_OF_SOURCES; src++) {
            if ((eb->eb_mode == 0x0) ||
                ((eb->eb_mode & 0x1) &&
                 (eb->pnd_src & mask))) {
                done2 = 0;
                while ((eb->q_buf_i_rd[src] !=
                        eb->q_buf_i_wr[src]) &&
                       (!done2)) {
                    // When event builder is active, check
                    // if we have the Start Of Built Event
                    if ((eb->eb_mode & 0x1) &&
                        (eb->had_sobe == 0)) {
                        // Emit start of built event
                        if ((err =
                                     EventBuilder_EmitEventBoundary(
                                             eb, 0)) < 0) {
                            printf(
                                    "EventBuilder_Loop: "
                                    "EventBuilder_"
                                    "EmitEventBoundary failed "
                                    "%d\n",
                                    err);
                            return (err);
                        } else {
                            eb->had_sobe = 1;
                            // printf("EventBuilder_Loop:
                            // start building new event\n");
                        }
                    }

                    // Get the buffer for the input queue of
                    // the current source
                    buf = eb->q_buf_i[src]
                                     [eb->q_buf_i_rd[src]];
                    // printf("EventBuilder_Loop: processing
                    // buffer 0x%x Source:%d i_rd=%d i_wr=%d
                    // i_sz=%d\n", buf, src,
                    // eb->q_buf_i_rd[src],
                    // eb->q_buf_i_wr[src],
                    // eb->q_buf_i_sz[src]);
                    eb->q_buf_i_rd[src] =
                            (eb->q_buf_i_rd[src] + 1) %
                            MAX_QUEUE_SIZE;
                    eb->q_buf_i_sz[src] =
                            eb->q_buf_i_sz[src] - 1;

                    // Check the content of this buffer
                    if ((err = EventBuilder_CheckBuffer(
                                 eb, src, buf)) < 0) {
                        printf(
                                "EventBuilder_Loop: "
                                "EventBuilder_CheckBuffer "
                                "failed %d\n",
                                err);
                        return (err);
                    }

                    // Process the current buffer
                    if ((err = EventBuilder_ProcessBuffer(
                                 eb, buf)) < 0) {
                        printf(
                                "EventBuilder_Loop: "
                                "EventBuilder_ProcessBuffer "
                                "failed %d\n",
                                err);
                        return (err);
                    }

                    // If this has and end of event, we do
                    // not wait more data from this FEM for
                    // this event
                    if (Frame_IsDFrame_EndOfEvent(buf)) {
                        eb->pnd_src &= ~mask;
                        // printf("EventBuilder_Loop: FEM %d
                        // had EOE pnd_src=0x%x\n", src,
                        // eb->pnd_src);
                        done2 = 1;
                    } else {
                    }

                    // Append this buffer to the ouptut
                    // queue of the event builder
                    if (((eb->q_buf_o_wr + 1) %
                         MAX_QUEUE_SIZE) ==
                        eb->q_buf_o_rd) {
                        printf(
                                "EventBuilder_Loop: q_buf_o "
                                "full\n");
                        err = -1;
                        return (err);
                    } else {
                        eb->q_buf_o[eb->q_buf_o_wr] = buf;
                        eb->q_src_o[eb->q_buf_o_wr] = src;
                        eb->q_buf_o_wr =
                                (eb->q_buf_o_wr + 1) %
                                MAX_QUEUE_SIZE;
                        eb->q_buf_o_sz = eb->q_buf_o_sz + 1;
                        // printf("EventBuilder_Loop: added
                        // buffer 0x%x src=%d o_rd=%d
                        // o_wr=%d o_sz=%d\n", buf, src,
                        // eb->q_buf_o_rd, eb->q_buf_o_wr,
                        // eb->q_buf_o_sz);
                    }
                    had_buf = 1;
                }
            }
            mask <<= 1;
        }

        // If the event builder is active, check for event
        // assembly done
        if (eb->eb_mode & 0x1) {
            if (eb->pnd_src == 0) {
                // Emit end of built event
                if ((err = EventBuilder_EmitEventBoundary(
                             eb, 1)) < 0) {
                    printf(
                            "EventBuilder_Loop: "
                            "EventBuilder_EmitEventBoundary "
                            "failed %d\n",
                            err);
                    return (err);
                } else {
                    eb->had_sobe = 0;

                    auto& storage_manager = feminos_daq_storage::StorageManager::Instance();

                    if (storage_manager.IsInitialized()) {

                        // Send a special frame signaling the end of a built event
                        storage_manager.AddFrame({0});

                        if (storage_manager.GetNumberOfEntries() == 0) {
                            storage_manager.millisSinceEpochForSpeedCalculation = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                        }
                    }
                }

                // Next event does not have any Start of Event received yet
                eb->src_had_soe = 0;
            } else {
                // printf("EventBuilder_Loop: pending source pattern 0x%x\n", eb->pnd_src);
            }
        }

        // if there was no buffer
        if (had_buf == 0) {
            // printf("EventBuilder_Loop: had not buffer:
            // sleeping 1s\n"); Sleep(1000);
        }

        // Get mutex to send over the network
        if ((err = Mutex_Lock(fa->snd_mutex)) < 0) {
            printf(
                    "FemArray_EventBuilderIO: Mutex_Lock "
                    "failed %d\n",
                    err);
            return (err);
        }

        // Recycle all the buffers which are no longer used
        // by the Event Builder
        done = 0;
        while (!done) {
            if ((err = EventBuilder_GetBufferToRecycle(
                         fa->eb, &buf, &fem_src)) < 0) {
                printf(
                        "EventBuilder_Loop: "
                        "EventBuilder_GetBufferToRecycle "
                        "failed %d\n",
                        err);
                return (err);
            }
            if (buf) {
                // Get the size of the buffer
                sz = (unsigned short*) buf;
                len = (int) (*sz);

                // Update the global size of data received
                // and left to collect
                fa->daq_size_rcv += len;
                fa->daq_size_left -= len;
                if (fa->daq_size_left < 0) {
                    fa->daq_size_left = 0;
                }

                // Update the credit of the FEM from which
                // this buffer was received
                if (fa->cred_unit == 'B') {
                    fa->fp[fem_src].req_credit += len;
                    fa->fp[fem_src].pnd_recv -= len;
                } else {
                    fa->fp[fem_src].req_credit++;
                    fa->fp[fem_src].pnd_recv--;
                }
                if (fa->fp[fem_src].pnd_recv < 0) {
                    fa->fp[fem_src].pnd_recv = 0;
                }
                /*
                printf("EventBuilder_Loop: fem(%d):
                pnd_recv=%d req_credit=%d
                fa->daq_size_left=%d\n", fem_src,
                        fa->fp[fem_src].pnd_recv,
                        fa->fp[fem_src].req_credit,
                        fa->daq_size_left);
                */
                // Return the buffer to the pool
                BufPool_ReturnBuffer(fa->bp,
                                     (unsigned long) buf);
            } else {
                done = 1;
            }
        }

        // Release mutex to send over the network
        if ((err = Mutex_Unlock(fa->snd_mutex)) < 0) {
            printf(
                    "FemArray_EventBuilderIO: Mutex_Unlock "
                    "failed %d\n",
                    err);
            return (err);
        }

        // If one or several event data frames were received
        // if (was_event_data)
        {
            // Try to post some requests to avoid starving
            sprintf(cmd, "DAQ -2\n");
            if ((err = FemArray_SendDaq(
                         fa, 0, 31, fa->fem_proxy_set, cmd)) <
                0) {
                return (err);
            }
        }

        // Release exclusive access to event builder queues
        if ((err = Mutex_Unlock(eb->q_mutex)) < 0) {
            printf(
                    "EventBuilder_Loop: Mutex_Unlock failed "
                    "%d\n",
                    err);
            return (err);
        }
    }

    printf("EventBuilder_Loop: completed.\n");

    return (err);
}

/*******************************************************************************
 EventBuilder_PutBufferToProcess
*******************************************************************************/
int EventBuilder_PutBufferToProcess(EventBuilder* eb,
                                    void* bufi, int src) {
    int err = 0;

    // Check that this input queue is not full
    if (((eb->q_buf_i_wr[src] + 1) % MAX_QUEUE_SIZE) ==
        eb->q_buf_i_rd[src]) {
        printf(
                "EventBuilder_PutBufferToProcess: Queue %d is "
                "full!\n",
                src);
        err = -1;
    } else {
        eb->q_buf_i[src][eb->q_buf_i_wr[src]] = bufi;
        eb->q_buf_i_wr[src] =
                (eb->q_buf_i_wr[src] + 1) % MAX_QUEUE_SIZE;
        eb->q_buf_i_sz[src] = eb->q_buf_i_sz[src] + 1;
        // printf("EventBuilder_PutBufferToProcess: added
        // buffer 0x%x Queue %d i_rd=%d i_wr=%d i_sz=%d\n",
        // bufi, src, eb->q_buf_i_rd[src],
        // eb->q_buf_i_wr[src], eb->q_buf_i_sz[src]);
        err = 0;
    }
    return (err);
}

/*******************************************************************************
 EventBuilder_GetBufferToRecycle
*******************************************************************************/
int EventBuilder_GetBufferToRecycle(EventBuilder* eb,
                                    void** bufo, int* src) {
    if (eb->q_buf_o_rd == eb->q_buf_o_wr) {
        *bufo = (void*) nullptr;
        *src = -1;
        return (0);
    } else {
        *bufo = eb->q_buf_o[eb->q_buf_o_rd];
        *src = eb->q_src_o[eb->q_buf_o_rd];
        eb->q_buf_o_rd =
                (eb->q_buf_o_rd + 1) % MAX_QUEUE_SIZE;
        eb->q_buf_o_sz = eb->q_buf_o_sz - 1;
        // printf("EventBuilder_GetBufferToRecycle: recycle
        // buffer 0x%x from Source %d\n", *bufo, *src);
        return (0);
    }
}

/*******************************************************************************
 EventBuilder_FileAction
*******************************************************************************/
int EventBuilder_FileAction(EventBuilder* eb,
                            EBFileActions action,
                            int format) {
    if (readOnly) { return 0; }

    struct tm* now;
    char name[200];
    time_t start_time;
    char str_res[4];
    char str_ext[8];
    unsigned short hdr;
    int i;
    int len;

    int err = 0;

    FILE* anFiles;
    char fileAnalysis[256];
    const auto files_to_analyse_path = getenv("FILES_TO_ANALYSE_PATH");

    int tt;

    // Close the last file
    if (action == EBFA_CloseLast) {
        if (eb->fout == 0) {
            printf("Warning: no file is open\n");
        } else {
            fflush(eb->fout);
            fclose(eb->fout);

            if (files_to_analyse_path) {
                // Adding file to the analysis queue
                sprintf(fileAnalysis, "%s/%s",
                        files_to_analyse_path,
                        fileNameNow);

                anFiles = fopen(fileAnalysis, "wt");
                if (anFiles) {
                    fclose(anFiles);
                } else {
                    printf("Error adding file to the analysis queue: %s\n",
                           fileAnalysis);
                }

                // Adding file to signal the end of the run
                sprintf(fileAnalysis, "%s/%s",
                        files_to_analyse_path,
                        fileNameEndRun);

                anFiles = fopen(fileAnalysis, "wt");
                if (anFiles) {
                    fclose(anFiles);
                } else {
                    printf("Error adding end of run file to the analysis queue: %s\n",
                           fileAnalysis);
                }
            }

            eb->fout = (FILE*) nullptr;

            if (eb->savedata == 1) {
                printf("File closed\n");
            } else if (eb->savedata == 2) {
                printf(
                        "File closed (size: %d MB   %d "
                        "bytes)\n",
                        eb->byte_wr / (1024 * 1024),
                        eb->byte_wr);
            }
            eb->savedata = 0;
        }

        return (0);
    }
    // Close the current file
    else if (action == EBFA_CloseCurrentOpenNext) {
        if (eb->fout == nullptr) {
        } else {
            fflush(eb->fout);
            fclose(eb->fout);

            if (files_to_analyse_path) {
                sprintf(fileAnalysis, "%s/%s",
                        files_to_analyse_path,
                        fileNameNow);
                anFiles = fopen(fileAnalysis, "wt");
                if (anFiles) {
                    fclose(anFiles);
                } else {
                    printf("Error adding file to the analysis queue: %s\n",
                           fileAnalysis);
                }
            }

            eb->fout = (FILE*) nullptr;
        }
    }

    // ASCII format
    if (format == 1) {
        sprintf(str_res, "w");
        sprintf(str_ext, "txt");
    }
    // Binary format
    else if (format == 2) {
        sprintf(str_res, "wb");
        sprintf(str_ext, "aqs");
    }

    if (action == EBFA_OpenFirst) {
        // Get the time of start
        time(&start_time);
        now = localtime(&start_time);

        tt = (int) time(nullptr);
        timeStart = tt;
        printf("Starting timestamp : %d\n", tt);

        // Clear the run string
        for (i = 0; i < 40; i++) {
            eb->run_str[i] = 0;
        }

        // Prepare the run string
        /* Old format filename
           sprintf(eb->run_str,
           "R%4d_%02d_%02d-%02d_%02d_%02d",
           ((now->tm_year)+1900),
           ((now->tm_mon)+1),
           now->tm_mday,
           now->tm_hour,
           now->tm_min,
           now->tm_sec);
         */
        sprintf(eb->run_str,
                "R%05d_%s_Vm_%s_Vd_%s_Pr_%s_Gain_%s_Shape_%s_Clock_%s",
                runNumber, runTagStr, meshVoltageStr,
                driftFieldStr, detectorPressureStr, gainStr,
                shapingStr, clockStr);

        FILE* felog = fopen("/tmp/elog.file", "wt");

        fprintf(felog, "%s\n", runComments);
        fprintf(felog, "Vmesh : %s V\n", meshVoltageStr);
        fprintf(felog, "Vdrift : %s V/cm/bar\n",
                driftFieldStr);
        fprintf(felog, "AGET gain : %s\n", gainStr);
        fprintf(felog, "AGET shaping : %s\n", shapingStr);
        fprintf(felog, "AGET clock : %s\n", clockStr);

        fclose(felog);

        char elogActive[64];

        if (getenv("ELOG_ACTIVE")) {
            sprintf(elogActive, "%s", getenv("ELOG_ACTIVE"));
        } else {
            sprintf(elogActive, "%s", "OFF");
        }
        printf("elogActive:%s\n", elogActive);

        if (strstr(elogActive, "YES") != NULL) {
            char elogName[64];
            sprintf(elogName, "%s", getenv("ELOG_NAME"));
            char elogIP[64];
            sprintf(elogIP, "%s", getenv("ELOG_IP"));
            char elogPORT[64];
            sprintf(elogPORT, "%s", getenv("ELOG_PORT"));

            if (strstr(runTagStr, "Test") == NULL &&
                strstr(runTagStr, "test") == NULL) {
                sprintf(elogCommand,
                        "cat /tmp/elog.file | elog -h %s "
                        "-p %s -l %s -a Type=DataTaking -a "
                        "Detector=%s -a Author=DAQ -a "
                        "Subject=\"Run#%05d - %s\"",
                        elogIP, elogPORT, elogName,
                        detectorStr, runNumber, runTagStr);
                printf("Launching eLog command : \n",
                       elogCommand);
                printf("%s\n", elogCommand);

                system(elogCommand);
            }
        }

        // Clear sub-run index
        eb->subrun_ix = 0;
    } else if (action == EBFA_CloseCurrentOpenNext) {
        eb->subrun_ix++;
    }

    const auto output_directory = feminos_daq_storage::StorageManager::Instance().GetOutputDirectory();
    sprintf(&(eb->file_path[0]), "%s", output_directory.c_str());

    auto& storage_manager = feminos_daq_storage::StorageManager::Instance();
    if (!storage_manager.output_filename_manual.empty()) {
        // clear strings (c-string)
        eb->file_path[0] = '.';
        eb->file_path[1] = '/';
        eb->file_path[2] = '\0';
        storage_manager.output_filename_manual.copy(eb->run_str, storage_manager.output_filename_manual.size());
        eb->run_str[storage_manager.output_filename_manual.size()] = '\0';
    }
    char filename_root[200] = {};
    sprintf(filename_root, "%s/%s.%s", &(eb->file_path[0]),
            &(eb->run_str[0]), "root");
    cout << "Root file name : " << filename_root << endl;
    sprintf(name, "%s/%s-%03d.%s", &(eb->file_path[0]),
            &(eb->run_str[0]), eb->subrun_ix, str_ext);
    sprintf(fileNameNow, "%s-%03d.%s", &(eb->run_str[0]),
            eb->subrun_ix, str_ext);
    sprintf(fileNameEndRun, "%s-%03d.%s", &(eb->run_str[0]),
            eb->subrun_ix, "endRun");

    if (action == EBFA_OpenFirst) {
        printf("Opened result file: \"%s\"\n", name);
    }

    // Open result file
    if (!(eb->fout = fopen(name, str_res))) {
        printf(
                "EventBuilder_FileAction: could not open file "
                "%s.\n",
                name);
        return (-1);
    }

    printf("Opening file : %s\n", name);

    // This loop is entered for every subrun, so we need to make sure this is only initialized once
    if (!storage_manager.IsInitialized()) {
        storage_manager.Initialize(filename_root);

        storage_manager.run_number = runNumber;
        storage_manager.run_name = eb->run_str;
        storage_manager.run_tag = runTagStr;
        storage_manager.run_comments = runComments;
        storage_manager.run_tag = runTagStr;

        try {
            storage_manager.run_drift_field_V_cm_bar = atof(driftFieldStr);
            storage_manager.run_mesh_voltage_V = atof(meshVoltageStr);
            storage_manager.run_detector_pressure_bar = atof(detectorPressureStr);
        } catch (std::exception& e) {
            cout << e.what() << endl;
            cout << "Error parsing run parameters. Some of these parameters couldn't be converted into float." << endl;
            cout << "Drift field: " << driftFieldStr << endl;
            cout << "Mesh voltage: " << meshVoltageStr << endl;
            cout << "Detector pressure: " << detectorPressureStr << endl;
        }

        storage_manager.run_tree->Fill();

        feminos_daq_prometheus::PrometheusManager::Instance().SetRunNumber(runNumber);
    }

    // in ASCII format add a carriage return
    if (format == 1) {
        sprintf(name, "RUN : %s-%03d\n", eb->run_str,
                eb->subrun_ix);
        len = strlen(name);

        // Write run string to file
        fwrite(name, len, 1, eb->fout);
        eb->byte_wr += len;
    }
    // in binary format add a ASCII prefix
    else if (format == 2) {
        sprintf(name, "%s-%03d", eb->run_str,
                eb->subrun_ix);
        len = strlen(name);

        // String should include null termination and size
        // must be even
        if ((len % 2) == 0) {
            strcat(name, " ");
            len += 2;
        } else {
            len += 1;
        }

        // Prepare ASCII prefix and write it to file
        hdr = PUT_ASCII_LEN(len);
        fwrite(&hdr, 2, 1, eb->fout);
        eb->byte_wr += 2;

        fwrite(&timeStart, sizeof(int), 1, eb->fout);
        eb->byte_wr += sizeof(int);
        /*

        // Write run string to file
        fwrite(name, len, 1, eb->fout);
        eb->byte_wr+=len;
         */
    }

    eb->savedata = format;
    eb->byte_wr = 0;

    return (err);
}
