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
#include <string>
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

void removeRootExtension(std::string& filename) {
    const std::string extension = ".root";
    std::size_t pos = filename.rfind(extension);
    if (pos != std::string::npos && pos == filename.length() - extension.length()) {
        filename.erase(pos); // Remove the extension
    }
}

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
    bool version_flag = false;
    bool enable_aqs = false;
    std::string compression_option = "default";
    double stop_run_after_seconds = 0;
    unsigned int stop_run_after_entries = 0;
    bool allow_losing_events = false;
    bool skip_run_info = false;

    CLI::App app{"feminos-daq"};

    app.add_flag("--version", version_flag, "Print the version");
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
    app.add_option("-o,--output", output_file, "Save results in file specified")
            ->group("File Options");
    app.add_option("-d,--output-directory", output_directory, "Output directory. This can also be specified via the environment variable 'FEMINOS_DAQ_OUTPUT_DIRECTORY' or 'RAWDATA_PATH'")
            ->group("File Options");
    app.add_option("-v,--verbose", verbose_level, "Verbose level")
            ->group("General")
            ->check(CLI::Range(0, 4));
    app.add_option("-t,--time", stop_run_after_seconds, "Stop the acquisition after the specified time in seconds")
            ->group("General")
            ->check(CLI::Range(0.0, 1e6));
    app.add_option("-e,--entries", stop_run_after_entries, "Stop the acquisition after reaching the specified number of entries")
            ->group("General");
    app.add_flag("--read-only", readOnly, ("Read-only mode"))
            ->group("General");
    app.add_flag("--allow-losing-events", allow_losing_events, "Allow losing events if the buffer is full (acceptable for calibrations, not for background runs)")
            ->group("General");
    app.add_flag("--shared-buffer", sharedBuffer, "Store event data in a shared memory buffer")->group("General");
    app.add_flag("--compression", compression_option,
                 R"(Select the compression settings for the output root file. Data must be written to disk faster than it is acquired. Frames are never dropped, if the rate is too high (or the disk too slow) a queue will begin to fill up and a warning message will appear.
- fast: fastest compression, use when the acquisition rate is very high (e.g. calibration runs)
- highest: best compression, use when the acquisition rate is low (e.g. background runs). To use whenever possible
- default: default compression settings, a balance between speed and compression)")
            ->group("File Options")
            ->check(CLI::IsMember(feminos_daq_storage::StorageManager::GetCompressionOptions()));
    app.add_flag("--enable-aqs", enable_aqs, "Store data in aqs format")->group("File Options");
    app.add_flag("--skip-run-info", skip_run_info, "Skip asking for run information and use default values (same as pressing enter)")->group("General");

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
    storage_manager.compression_option = compression_option;
    storage_manager.disable_aqs = !enable_aqs;
    storage_manager.stop_run_after_seconds = stop_run_after_seconds;
    storage_manager.stop_run_after_entries = stop_run_after_entries;
    storage_manager.allow_losing_events = allow_losing_events;
    storage_manager.skip_run_info = skip_run_info;

    stringIpToArray(server_ip, femarray.rem_ip_beg);
    stringIpToArray(local_ip, femarray.loc_ip);

    if (!output_file.empty()) {
        removeRootExtension(output_file);
        storage_manager.output_filename_manual = output_file;
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

    // Create Event Builder thread
    eventbuilder.thread.routine = reinterpret_cast<void (*)()>(EventBuilder_Loop);
    eventbuilder.thread.param = (void*) &eventbuilder;
    eventbuilder.state = 1;
    if ((err = Thread_Create(&eventbuilder.thread)) < 0) {
        printf("Thread_Create failed %d\n", err);
        goto cleanup;
    }

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
