/*******************************************************************************

 File:        mclientUDP.c

 Description: A simple UDP client application for testing data acquisition
 of Feminos cards over Fast/Gigabit Ethernet.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
   May 2011 : copied from T2K version

  April 2013: added -c option to specify the IP address of the local interface
  to be used for communication with the Feminos.

*******************************************************************************/

#include "bufpool.h"
#include "cmdfetcher.h"
#include "evbuilder.h"
#include "femarray.h"
#include "frame.h"
#include "os_al.h"
#include "platform_spec.h"
#include "sock_util.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <thread>

#include "graph.h"
#include "prometheus.h"
#include "storage.h"

/*******************************************************************************
 Constants types and global variables
*******************************************************************************/
char res_file[80] = {"\0"};
int save_res = 0;
int verbose = 4;
int format_ver = 2;
int pending_event = 0;
int comp = 0;
int echo_cmd = 1;

int cur_fem;

CmdFetcher cmdfetcher;
FemArray femarray;
BufPool bufpool;
EventBuilder eventbuilder;

/*******************************************************************************
 * Variables associated to shared memory buffer
 *******************************************************************************/
int shareBuffer = 0;
int readOnly = 0;
int tcm = 0;

int ShMem_Buffer_ID;
int ShMem_DaqInfo_ID;
daqInfo* ShMem_DaqInfo;
unsigned short int* ShMem_Buffer;
int SemaphoreId;

const int MAX_SIGNALS = 1152; // To cover up to 4 Feminos boards 72 * 4 * 4
const int MAX_POINTS = 512;

/*******************************************************************************
 help() to display usage
*******************************************************************************/
void help() {
    printf("clientUDP <options>\n");
    printf("   -h               : print this message help\n");
    printf("   -s <xx.yy.zz.ww> : base IP address of remote server(s) in dotted decimal\n");
    printf("   -p <port>        : remote UDP target port\n");
    printf("   -S <0xServer_set>: hexadecimal pattern to tell which server(s) to connect to\n");
    printf("   -c <xx.yy.zz.ww> : IP address of the local interface in dotted decimal\n");
    printf("   -f <file>        : read commands from file specified\n");
    printf("   -o <file>        : save results in file specified\n");
    printf("   -v <level>       : verbose\n");
}

/*******************************************************************************
 parse_cmd_args() to parse command line arguments
*******************************************************************************/
int parse_cmd_args(int argc, char** argv) {
    int i;
    int match;
    int err = 0;

    for (i = 1; i < argc; i++) {
        match = 0;
        // remote host IP address
        if (strncmp(argv[i], "-s", 2) == 0) {
            match = 1;
            if ((i + 1) < argc) {
                i++;
                if (sscanf(argv[i], "%d.%d.%d.%d", &(femarray.rem_ip_beg)[0], &(femarray.rem_ip_beg[1]),
                           &(femarray.rem_ip_beg[2]), &(femarray.rem_ip_beg[3])) == 4) {
                } else {
                    printf("illegal argument %s\n", argv[i]);
                    return (-1);
                }
            } else {
                printf("missing argument after %s\n", argv[i]);
                return (-1);
            }
        }
        // remote port number
        else if (strncmp(argv[i], "-p", 2) == 0) {
            match = 1;
            if ((i + 1) < argc) {
                i++;
                if (sscanf(argv[i], "%d", &femarray.rem_port) == 1) {

                } else {
                    printf("illegal argument %s\n", argv[i]);
                    return (-1);
                }
            } else {
                printf("missing argument after %s\n", argv[i]);
                return (-1);
            }
        }
        // Server pattern
        else if (strncmp(argv[i], "-S", 2) == 0) {
            match = 1;
            if ((i + 1) < argc) {
                i++;
                if (sscanf(argv[i], "0x%x", &femarray.fem_proxy_set) == 1) {
                    femarray.fem_proxy_set &= 0xFFFFFFFF;
                } else {
                    printf("illegal argument %s\n", argv[i]);
                    return (-1);
                }
            } else {
                printf("missing argument after %s\n", argv[i]);
                return (-1);
            }
        }
        // local IP address of the interface
        if (strncmp(argv[i], "-c", 2) == 0) {
            match = 1;
            if ((i + 1) < argc) {
                i++;
                if (sscanf(argv[i], "%d.%d.%d.%d", &(femarray.loc_ip[0]), &(femarray.loc_ip[1]), &(femarray.loc_ip[2]),
                           &(femarray.loc_ip[3])) == 4) {

                } else {
                    printf("illegal argument %s\n", argv[i]);
                    return (-1);
                }
            } else {
                printf("missing argument after %s\n", argv[i]);
                return (-1);
            }
        }
        // get file name for commands
        else if (strncmp(argv[i], "-f", 2) == 0) {
            match = 1;
            if ((i + 1) < argc) {
                i++;
                strcpy(&(cmdfetcher.cmd_file[0]), argv[i]);
                cmdfetcher.use_stdin = 0;
            } else {
                printf("missing argument after %s\n", argv[i]);
                return (-1);
            }
        }
        // result file name
        else if (strncmp(argv[i], "-o", 2) == 0) {
            match = 1;
            if ((i + 1) < argc) {
                i++;
                strcpy(res_file, argv[i]);
                save_res = 1;
            } else {
                printf("missing argument after %s\n", argv[i]);
                return (-1);
            }
        }
        // format for saving data
        else if (strncmp(argv[i], "-F", 2) == 0) {
            match = 1;
            if ((i + 1) < argc) {
                if (sscanf(argv[i + 1], "%d", &format_ver) == 1) {
                    i++;
                } else {
                    format_ver = 2;
                }
            } else {
                format_ver = 2;
            }
        }
        // verbose
        else if (strncmp(argv[i], "-v", 2) == 0) {
            match = 1;
            if ((i + 1) < argc) {
                if (sscanf(argv[i + 1], "%d", &(cmdfetcher.verbose)) == 1) {
                    i++;
                } else {
                    cmdfetcher.verbose = 1;
                }
            } else {
                cmdfetcher.verbose = 1;
            }
            femarray.verbose = cmdfetcher.verbose;

        } else if (strncmp(argv[i], "-h", 2) == 0) {
            match = 1;
            help();
            return (-1); // force an error to exit
        } else if (strncmp(argv[i], "shareBuffer", 11) == 0) {
            match = 1;
            shareBuffer = 1;
        } else if (strncmp(argv[i], "readOnly", 8) == 0) {
            match = 1;
            readOnly = 1;
        } else if (strncmp(argv[i], "tcm", 3) == 0) {
            match = 1;
            tcm = 1;
        } else if (strncmp(argv[i], "RST", 3) == 0) {
            match = 1;
            tcm = 1;
            shareBuffer = 1;
            readOnly = 1;
        } else if (strncmp(argv[i], "ST", 3) == 0) {
            match = 1;
            tcm = 1;
            shareBuffer = 1;
        }
        // unmatched options
        if (match == 0) {
            printf("Warning: unsupported option %s\n", argv[i]);
        }
    }
    return (0);
}

void CleanSharedMemory(int s) {
    printf("Cleaning shared resources\n");

    int err = shmctl(ShMem_Buffer_ID, IPC_RMID, nullptr);
    err = shmctl(ShMem_DaqInfo_ID, IPC_RMID, nullptr);

    semctl(SemaphoreId, 0, IPC_RMID, 0);

    exit(1);
}

/*******************************************************************************
 Main
*******************************************************************************/
int main(int argc, char** argv) {

    // prometheus manager
    auto& prometheus_manager = mclient_prometheus::PrometheusManager::Instance();
    auto& storage_manager = mclient_storage::StorageManager::Instance();
    auto& graph_manager = mclient_graph::GraphManager::Instance();

    /*
    int eventId = 0;
    while (true) {
        storage_manager.Clear();
        auto& event = storage_manager.event;
        event.id = eventId++;

        // random between 1 and 5
        int nSignals = rand() % 5 + 1;
        for (int i = 0; i < nSignals; i++) {
            unsigned short id = rand() % 1000;
            std::array<unsigned short, mclient_storage::Event::SIGNAL_SIZE> data{};
            for (int j = 0; j < mclient_storage::Event::SIGNAL_SIZE; j++) {
                data[j] = rand() % 4096;
            }
            event.add_signal(id, data);
        }

        cout << "Event ID: " << storage_manager.event.id << endl;

        graph_manager.DrawEvent(event);

        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
    */

    int err;

    // Initialize Command Fetcher
    CmdFetcher_Init(&cmdfetcher);
    cmdfetcher.verbose = verbose;

    // Initialize FEM array
    FemArray_Clear(&femarray);

    // Initialize Event Builder
    EventBuilder_Clear(&eventbuilder);

    // Initialize socket library
    if ((err = socket_init()) < 0) {
        printf("sock_init failed: %d\n", err);
        return (err);
    }

    // Parse command line arguments
    if (parse_cmd_args(argc, argv) < 0) {
        printf("parse_cmd_args failed: %d\n", err);
        return (-1);
    }

    ////////////////////////////////////////////////

    /* Creating shared memory to dump event data */
    if (shareBuffer) {

        key_t MemKey = ftok("/bin/ls", 3);
        ShMem_DaqInfo_ID = shmget(MemKey, sizeof(daqInfo), 0777 | IPC_CREAT);
        ShMem_DaqInfo = (daqInfo*) shmat(ShMem_DaqInfo_ID, nullptr, 0);

        ShMem_DaqInfo->maxSignals = MAX_SIGNALS;
        ShMem_DaqInfo->maxSamples = MAX_POINTS;
        ShMem_DaqInfo->timeStamp = 0;
        ShMem_DaqInfo->dataReady = 0;
        ShMem_DaqInfo->nSignals = 0;
        ShMem_DaqInfo->eventId = 0;

        int N_DATA = MAX_SIGNALS * (MAX_POINTS + 1); // We add 1-point to store the daqChannel Id
        ShMem_DaqInfo->bufferSize = N_DATA;

        MemKey = ftok("/bin/ls", 13);
        ShMem_Buffer_ID = shmget(MemKey, N_DATA * sizeof(unsigned short int), 0777 | IPC_CREAT);
        ShMem_Buffer = (unsigned short int*) shmat(ShMem_Buffer_ID, nullptr, 0);

        for (int n = 0; n < N_DATA; n++) { ShMem_Buffer[n] = 0; }

        // Creating semaphores to handle access to shared memory

        key_t SemaphoreKey = ftok("/bin/ls", 14);
        SemaphoreId = semget(SemaphoreKey, 1, 0777 | IPC_CREAT);
        semctl(SemaphoreId, 0, SETVAL, 1);

        signal(SIGINT, CleanSharedMemory);
    }

    // Initialize Buffer Pool
    BufPool_Init(&bufpool);

    // Open the array of FEM
    if ((err = FemArray_Open(&femarray)) < 0) {
        printf("FemArray_Open failed: %d\n", err);
        goto cleanup;
    }

    // Open the Event Builder
    if ((err = EventBuilder_Open(&eventbuilder)) < 0) {
        printf("EventBuilder_Open failed: %d\n", err);
        goto cleanup;
    }

    // Pass a pointer to the fem array to the command catcher
    cmdfetcher.fa = (void*) &femarray;

    // Pass a pointer to buffer pool and event builder to the fem array
    femarray.bp = (void*) &bufpool;
    femarray.eb = (void*) &eventbuilder;

    // Pass a pointer to the fem array to the event builder
    eventbuilder.fa = (void*) &femarray;

    // Create FEM Array thread
    femarray.thread.routine = (void*) FemArray_ReceiveLoop;
    femarray.thread.param = (void*) &femarray;
    femarray.state = 1;
    if ((err = Thread_Create(&femarray.thread)) < 0) {
        printf("Thread_Create failed %d\n", err);
        goto cleanup;
    }
    // printf("femarray Thread_Create done\n" );

    // Create Event Builder thread
    eventbuilder.thread.routine = (void*) EventBuilder_Loop;
    eventbuilder.thread.param = (void*) &eventbuilder;
    eventbuilder.state = 1;
    if ((err = Thread_Create(&eventbuilder.thread)) < 0) {
        printf("Thread_Create failed %d\n", err);
        goto cleanup;
    }
    // printf("eventbuilder Thread_Create done\n" );

    // Run the main loop of the command interpreter
    CmdFetcher_Main(&cmdfetcher);

    /* wait until FEM array thread stops */
    if (femarray.thread.thread_id >= 0) {
        if (Thread_Join(&femarray.thread) < 0) {
            printf("femarray: Thread_Join failed.\n");
        } else {
            printf("femarray: Thread_Join done.\n");
        }
    }

    /* wait until event builder thread stops */
    if (eventbuilder.thread.thread_id >= 0) {
        if (Thread_Join(&eventbuilder.thread) < 0) {
            printf("eventbuilder: Thread_Join failed.\n");
        } else {
            printf("eventbuilder: Thread_Join done.\n");
        }
    }

cleanup:

    socket_cleanup();

    if (shareBuffer) {
        CleanSharedMemory(0);
    }

    return (err);
}
