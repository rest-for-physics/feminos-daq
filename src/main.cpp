/*******************************************************************************

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

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <thread>

#include "prometheus.h"
#include "storage.h"

#include <CLI/CLI.hpp>

using namespace std;

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
int sharedBuffer = 0;
int readOnly = 0;
int tcm = 0;

int ShMem_Buffer_ID;
int ShMem_DaqInfo_ID;
daqInfo* ShMem_DaqInfo;
unsigned short int* ShMem_Buffer;
int SemaphoreId;

constexpr int MAX_SIGNALS = feminos_daq_storage::MAX_SIGNALS;
constexpr int MAX_POINTS = feminos_daq_storage::MAX_POINTS;

template<typename T>
void stringIpToArray(const std::string& ip, T* ip_array) {
    // we assume ip is a valid IP (this has to be checked beforehand)
    if (ip.empty()) {
        return;
    }

    std::vector<std::string> ip_parts;
    std::string delimiter = ".";
    std::string part;
    std::istringstream ip_stream(ip);

    // Split the IP address by the '.' delimiter
    while (std::getline(ip_stream, part, '.')) {
        ip_parts.push_back(part);
    }

    // Ensure we have exactly 4 parts
    if (ip_parts.size() != 4) {
        std::cerr << "Invalid IP address format" << std::endl;
        return;
    }

    // Convert the string parts to integers and store them in the array
    for (int i = 0; i < 4; i++) {
        ip_array[i] = std::stoi(ip_parts[i]);
    }
}

void CleanSharedMemory(int s) {
    printf("Cleaning shared resources\n");

    int err = shmctl(ShMem_Buffer_ID, IPC_RMID, nullptr);
    err = shmctl(ShMem_DaqInfo_ID, IPC_RMID, nullptr);

    semctl(SemaphoreId, 0, IPC_RMID, 0);

    exit(1);
}

int main(int argc, char** argv) {

    CmdFetcher_Init(&cmdfetcher);
    FemArray_Clear(&femarray);
    EventBuilder_Clear(&eventbuilder);

    std::string server_ip;
    std::string local_ip;
    std::string input_file;
    std::string output_file;
    int verbose_level = -1;
    std::string output_directory;
    std::string root_compression_algorithm = "LZMA";
    bool version_flag = false;

    CLI::App app{"feminos-daq"};

    app.add_option("-s,--server", server_ip, "Base IP address of remote server(s) in dotted decimal")
            ->group("Connection Options")
            ->check(CLI::ValidIPV4);
    app.add_option("-p,--port", femarray.rem_port, "Remote UDP target port")
            ->group("Connection Options")
            ->check(CLI::Range(1, 65535));
    app.add_option("-S,--servers", femarray.fem_proxy_set, "Hexadecimal pattern to tell which server(s) to connect to (e.g 0xC)")
            ->group("Connection Options")
            ->check(CLI::Number);
    app.add_option("-c,--client", local_ip, "IP address of the local interface in dotted decimal")
            ->group("Connection Options")
            ->check(CLI::ValidIPV4);
    app.add_option("-i,--input", input_file, "Read commands from file specified")
            ->group("File Options");
    app.add_option("-o,--output", output_file, "Save results in file specified (DOES NOT WORK!)")
            ->group("File Options");
    app.add_option("-d,--output-directory", output_directory, "Output directory. This can also be specified via the environment variable 'FEMINOS_DAQ_OUTPUT_DIRECTORY' or 'RAWDATA_PATH'")
            ->group("File Options");
    app.add_option("-v,--verbose", verbose_level, "Verbose level")
            ->group("General")
            ->check(CLI::Range(0, 4));
    app.add_flag("--read-only", readOnly, "Read-only mode")
            ->group("General");
    app.add_flag("--shared-buffer", sharedBuffer, "Store event data in a shared memory buffer")->group("General");
    app.add_option("--root-compression-algorithm", root_compression_algorithm, "Root compression algorithm. Use LZMA (default) for best compression or LZ4 for speed when the event rate very high")
            ->group("File Options")
            ->check(CLI::IsMember({"ZLIB", "LZMA", "LZ4"}));
    app.add_flag("--version", version_flag, "Print the version");

    CLI11_PARSE(app, argc, argv);

    if (version_flag) {
        std::cout << "feminos-daq version " << FEMINOS_DAQ_VERSION << std::endl;
        return 0;
    }

    if (verbose_level <= 0) {
        // not explicitly set, use default value (we need to put level 4 for initialization)
        verbose = 1;
    }

    femarray.verbose = verbose;
    cmdfetcher.verbose = verbose;

    auto& prometheus_manager = feminos_daq_prometheus::PrometheusManager::Instance();
    auto& storage_manager = feminos_daq_storage::StorageManager::Instance();

    storage_manager.SetOutputDirectory(output_directory);
    storage_manager.compression_algorithm = root_compression_algorithm;

    stringIpToArray(server_ip, femarray.rem_ip_beg);
    stringIpToArray(local_ip, femarray.loc_ip);

    if (!output_file.empty()) {
        if (output_file.length() > 80) {
            std::cerr << "Output file name is too long" << std::endl;
            return 1;
        }
        strcpy(res_file, output_file.c_str());
        save_res = 1;
    }

    if (!input_file.empty()) {
        if (input_file.length() > 80) {
            std::cerr << "Input file name is too long" << std::endl;
            return 1;
        }
        strcpy(cmdfetcher.cmd_file, input_file.c_str());
        cmdfetcher.use_stdin = 0;
    }

    int err = socket_init();
    if (err < 0) {
        std::cout << "socket_init failed: " << err << std::endl;
        return err;
    }

    if (sharedBuffer) {

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
    femarray.thread.routine = reinterpret_cast<void (*)()>(FemArray_ReceiveLoop);
    femarray.thread.param = (void*) &femarray;
    femarray.state = 1;
    if ((err = Thread_Create(&femarray.thread)) < 0) {
        printf("Thread_Create failed %d\n", err);
        goto cleanup;
    }
    // printf("femarray Thread_Create done\n" );

    // Create Event Builder thread
    eventbuilder.thread.routine = reinterpret_cast<void (*)()>(EventBuilder_Loop);
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

    if (sharedBuffer) {
        CleanSharedMemory(0);
    }

    return (err);
}
