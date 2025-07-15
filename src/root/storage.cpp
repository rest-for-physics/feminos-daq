
#include "storage.h"
#include "frame.h"
#include "prometheus.h"
#include <iostream>
#include <thread>

using namespace std;
using namespace feminos_daq_storage;

void feminos_daq_storage::StorageManager::Checkpoint(bool force) {
    if (!file) {
        return;
    }

    if (force || std::chrono::system_clock::now() - checkpoint_last > checkpoint_interval) {
        file->Write("", TObject::kOverwrite);
        checkpoint_last = std::chrono::system_clock::now();
    }
}

StorageManager::StorageManager() = default;

double StorageManager::GetSpeedEventsPerSecond() const {
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - millisSinceEpochForSpeedCalculation;
    if (millis <= 0) {
        return 0.0;
    }
    return 1000.0 * GetNumberOfEntries() / millis;
}

bool ReadFrame(const std::vector<unsigned short>& frame_data, feminos_daq_storage::Event& event) {
    unsigned short r0, r1, r2;
    unsigned short n0, n1;
    unsigned short cardNumber, chipNumber, daqChannel;
    unsigned int tmp;
    int tmp_i[10];
    int si = 0;

    auto p = const_cast<unsigned short*>(frame_data.data());
    auto start = p;

    bool end_of_event = false;
    bool done = false;

    unsigned int signal_id = 0;
    std::array<unsigned short, 512> signal_data = {};

    while (!done) {
        // Is it a prefix for 14-bit content?
        if ((*p & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_IX) {
            // if (sgnl.GetSignalID() >= 0 && sgnl.GetNumberOfPoints() >= fMinPoints) {                fSignalEvent->AddSignal(sgnl);            }

            if (si > 0) {
                event.add_signal(signal_id, signal_data);
            }

            cardNumber = GET_CARD_IX(*p);
            chipNumber = GET_CHIP_IX(*p);
            daqChannel = GET_CHAN_IX(*p);

            if (daqChannel >= 0) {
                daqChannel += cardNumber * 4 * 72 + chipNumber * 72;
            }

            p++;
            si = 0;

            signal_id = daqChannel;
        }
        // Is it a prefix for 12-bit content?
        else if ((*p & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE) {
            r0 = GET_ADC_DATA(*p);

            signal_data[si] = r0;

            p++;
            si++;
        }
        // Is it a prefix for 4-bit content?
        else if ((*p & PFX_4_BIT_CONTENT_MASK) == PFX_START_OF_EVENT) {
            // cout << " + Start of event" << endl;
            r0 = GET_EVENT_TYPE(*p);
            p++;

            // Time Stamp lower 16-bit
            r0 = *p;
            p++;

            // Time Stamp middle 16-bit
            r1 = *p;
            p++;

            // Time Stamp upper 16-bit
            r2 = *p;
            p++;

            // Set timestamp and event ID

            // Event Count lower 16-bit
            n0 = *p;
            p++;

            // Event Count upper 16-bit
            n1 = *p;
            p++;

            tmp = (((unsigned int) n1) << 16) | ((unsigned int) n0);

            // this is the time (in seconds) since the start of the acquisition by the Feminos
            auto time = (2147483648 * r2 + 32768 * r1 + r0) * 2e-8; // convert to seconds: 2147483648 = 2^31, 32768 = 2^15 used to scale r2 and r1 to build a 48-bit value and 20ns per clock tick

            // milliseconds unix time

            if (event.timestamp == 0) {
                // auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); // old way
                auto milliseconds = run_time_start_millis + static_cast<unsigned long long>(time * 1000);
                event.timestamp = milliseconds;
            }

        } else if ((*p & PFX_4_BIT_CONTENT_MASK) == PFX_END_OF_EVENT) {
            tmp = ((unsigned int) GET_EOE_SIZE(*p)) << 16;
            p++;
            tmp = tmp + (unsigned int) *p;
            p++;

            // if (fElectronicsType == "SingleFeminos") endOfEvent = true;
        }

        // Is it a prefix for 0-bit content?
        else if ((*p & PFX_0_BIT_CONTENT_MASK) == PFX_END_OF_FRAME) {
            // if (sgnl.GetSignalID() >= 0 && sgnl.GetNumberOfPoints() >= fMinPoints) { fSignalEvent->AddSignal(sgnl); }
            if (si > 0) {
                event.add_signal(signal_id, signal_data);
            }

            done = true;

            if (end_of_event) {
                cout << "End of frame fount at " << p - start << endl;
            }

            p++;

        } else if (*p == PFX_START_OF_BUILT_EVENT) {
            cout << "Start of built event fount at " << p - start << endl;
            p++;
        } else if (*p == PFX_END_OF_BUILT_EVENT) {
            end_of_event = true;
            cout << "End of event fount at " << p - start << endl;
            p++;
        } else if (*p == PFX_SOBE_SIZE) {
            // Skip header
            p++;

            // Built Event Size lower 16-bit
            r0 = *p;
            p++;
            // Built Event Size upper 16-bit
            r1 = *p;
            p++;
            tmp_i[0] = (int) ((r1 << 16) | (r0));
        } else {
            p++;
        }
    }
    return end_of_event;
}

void StorageManager::Initialize(const string& filename) {
    if (file != nullptr) {
        cerr << "StorageManager already initialized" << endl;
        throw std::runtime_error("StorageManager already initialized");
    }

    file = std::make_unique<TFile>(filename.c_str(), "RECREATE");

    if (compression_option == "default") {
        file->SetCompressionAlgorithm(ROOT::kLZMA); // biggest compression ratio but slowest
    } else if (compression_option == "fast") {
        // use root default compression
    } else if (compression_option == "highest") {
        file->SetCompressionAlgorithm(ROOT::kLZMA); // biggest compression ratio but slowest
        file->SetCompressionLevel(9);
    } else {
        throw std::runtime_error("Invalid compression option: " + compression_option);
    }

    cout << "ROOT file will be saved to " << file->GetName() << endl;

    event_tree = std::make_unique<TTree>("events", "Signal events. Each entry is an event which may contain multiple signals");

    event_tree->Branch("timestamp", &event.timestamp);
    event_tree->Branch("signal_ids", &event.signal_ids);
    event_tree->Branch("signal_values", &event.signal_values);

    run_tree = std::make_unique<TTree>("run", "Run metadata");

    run_tree->Branch("number", &run_number);
    run_tree->Branch("name", &run_name);
    run_tree->Branch("timestamp", &run_time_start_millis);
    run_tree->Branch("detector", &run_detector_name);
    run_tree->Branch("tag", &run_tag);
    run_tree->Branch("drift_field_V_cm_bar", &run_drift_field_V_cm_bar);
    run_tree->Branch("mesh_voltage_V", &run_mesh_voltage_V);
    run_tree->Branch("detector_pressure_bar", &run_detector_pressure_bar);
    run_tree->Branch("comments", &run_comments);
    run_tree->Branch("commands", &run_commands);

    // millis since epoch
    run_time_start_millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    auto& prometheus_manager = feminos_daq_prometheus::PrometheusManager::Instance();
    prometheus_manager.ExposeRootOutputFilename(filename);

    prometheus_manager.UpdateOutputRootFileSize();

    thread([this]() {
        while (true) {
            const auto frame = PopFrame();

            if (frame.empty()) {
                // PopFrame does not block since it requires locking the mutex. If there are no frames in the queue, it should return an empty frame
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else if (frame.size() == 1 && frame[0] == 0) {
                // special frame signaling end of built event
                auto& storage_manager = feminos_daq_storage::StorageManager::Instance();
                auto& prometheus_manager = feminos_daq_prometheus::PrometheusManager::Instance();

                if (storage_manager.IsInitialized()) {

                    storage_manager.event.id = storage_manager.event_tree->GetEntries();
                    storage_manager.event_tree->Fill();

                    storage_manager.Checkpoint();

                    prometheus_manager.SetNumberOfSignalsInEvent(storage_manager.event.size());
                    prometheus_manager.SetNumberOfEvents(storage_manager.event_tree->GetEntries());

                    prometheus_manager.UpdateOutputRootFileSize();

                    const bool exit_due_to_entries = storage_manager.stop_run_after_entries > 0 && storage_manager.event_tree->GetEntries() >= storage_manager.stop_run_after_entries;
                    const bool exit_due_to_time = storage_manager.stop_run_after_seconds > 0 && double(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) - double(storage_manager.run_time_start_millis) > storage_manager.stop_run_after_seconds * 1000.0;
                    if (exit_due_to_entries || exit_due_to_time) {
                        cout << "Stopping run at " << storage_manager.event_tree->GetEntries() << " entries" << endl;
                        early_exit();
                    }
                }

                storage_manager.Clear();
            } else {
                // read frame data into event
                ReadFrame(frame, event);
            }
        }
    }).detach();
}

void StorageManager::SetOutputDirectory(const string& directory) {
    // check it's a valid path, create it if it doesn't exist
    // if not specified, get it from env variable FEMINOS_DAQ_OUTPUT_DIRECTORY, then RAWDATA_PATH, otherwise use current directory

    if (directory.empty()) {
        const char* env = std::getenv("FEMINOS_DAQ_OUTPUT_DIRECTORY");
        if (env) {
            output_directory = env;
        } else {
            env = std::getenv("RAWDATA_PATH");
            if (env) {
                output_directory = env;
            }
        }
    } else {
        output_directory = directory;
    }

    if (output_directory.empty()) {
        output_directory = ".";
    }

    if (!std::filesystem::exists(output_directory)) {
        std::filesystem::create_directories(output_directory);
    }
}

void StorageManager::AddFrame(const vector<unsigned short>& frame) {
    lock_guard<mutex> lock(frames_mutex);
    frames.push(frame);
    frames_count++;

    if (frames.size() >= max_frames) {
        throw std::runtime_error("Too many frames in queue");
    }
}

std::vector<unsigned short> StorageManager::PopFrame() {
    lock_guard<mutex> lock(frames_mutex);
    if (frames.empty()) {
        return {};
    }
    auto frame = frames.front();
    frames.pop();
    return std::move(frame);
}

unsigned int StorageManager::GetNumberOfFramesInserted() const {
    return frames_count;
}

unsigned int StorageManager::GetNumberOfFramesInQueue() {
    lock_guard<mutex> lock(frames_mutex);
    return frames.size();
}

double StorageManager::GetQueueUsage() {
    return GetNumberOfFramesInQueue() / (double) max_frames;
}

void StorageManager::early_exit() const {
    // Invoking this from a thread is not the cleanest way to exit the program, but it appears to work

    if (file) {
        file->Write("", TObject::kOverwrite);
        file->Close();
    }

    exit(0);
}

std::pair<unsigned short, std::array<unsigned short, MAX_POINTS>> Event::get_signal_id_data_pair(size_t index) const {
    unsigned short channel = signal_ids[index];
    std::array<unsigned short, MAX_POINTS> data{};
    for (size_t i = 0; i < MAX_POINTS; ++i) {
        data[i] = signal_values[index * 512 + i];
    }
    return {channel, data};
}

void Event::add_signal(unsigned short id, const array<unsigned short, MAX_POINTS>& data) {
    signal_ids.push_back(id);
    signal_values.insert(signal_values.end(), data.begin(), data.end());
}
