
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
        file->Flush();
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

void ReadFrame(const std::vector<unsigned short>& frame_data, feminos_daq_storage::Event& event) {
    unsigned short r0, r1, r2;
    unsigned short n0, n1;
    unsigned short cardNumber, chipNumber, daqChannel;
    unsigned int tmp;
    int tmp_i[10];
    int si = 0;

    auto p = const_cast<unsigned short*>(frame_data.data());

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

            // auto time = 0 + (2147483648 * r2 + 32768 * r1 + r0) * 2e-8;

            // milliseconds unix time

            if (event.timestamp == 0) {
                auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
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

            p++;
            done = true;
        } else if (*p == PFX_START_OF_BUILT_EVENT) {
            p++;
        } else if (*p == PFX_END_OF_BUILT_EVENT) {
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
}

void StorageManager::Initialize(const string& filename) {
    if (file != nullptr) {
        cerr << "StorageManager already initialized" << endl;
        throw std::runtime_error("StorageManager already initialized");
    }

    file = std::make_unique<TFile>(filename.c_str(), "RECREATE");

    if (compression_algorithm == "ZLIB") {
        file->SetCompressionAlgorithm(ROOT::kZLIB); // good compression ratio and fast (old root default)
    } else if (compression_algorithm == "LZ4") {
        file->SetCompressionAlgorithm(ROOT::kLZ4); // good compression ratio and fast (new root default)
    } else if (compression_algorithm == "LZMA") {
        file->SetCompressionAlgorithm(ROOT::kLZMA); // biggest compression ratio but slowest
    } else {
        throw std::runtime_error("Unknown compression algorithm: " + compression_algorithm);
    }

    // file->SetCompressionLevel(9);               // max compression level, but it's very slow, probably not worth it

    cout << "ROOT file will be saved to " << file->GetName() << endl;

    event_tree = std::make_unique<TTree>("events", "Signal events. Each entry is an event which may contain multiple signals");

    event_tree->Branch("timestamp", &event.timestamp);
    event_tree->Branch("signal_ids", &event.signal_ids);
    event_tree->Branch("signal_values", &event.signal_values);

    run_tree = std::make_unique<TTree>("run", "Run metadata");

    run_tree->Branch("number", &run_number);
    run_tree->Branch("name", &run_name);
    run_tree->Branch("timestamp", &run_time_start);
    run_tree->Branch("detector", &run_detector_name);
    run_tree->Branch("tag", &run_tag);
    run_tree->Branch("drift_field_V_cm_bar", &run_drift_field_V_cm_bar);
    run_tree->Branch("mesh_voltage_V", &run_mesh_voltage_V);
    run_tree->Branch("detector_pressure_bar", &run_detector_pressure_bar);
    run_tree->Branch("comments", &run_comments);
    run_tree->Branch("commands", &run_commands);

    // millis since epoch
    run_time_start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    auto& prometheus_manager = feminos_daq_prometheus::PrometheusManager::Instance();
    prometheus_manager.ExposeRootOutputFilename(filename);

    prometheus_manager.UpdateOutputRootFileSize();

    // create a thread that pops frames
    thread([this]() {
        while (true) {
            auto frame = PopFrame();

            if (frame.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            ReadFrame(frame, event);
            event.clear();
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

    // pop oldest frames if we have too many
    while (frames.size() > 100000) {
        frames.pop();
    }
}

std::vector<unsigned short> StorageManager::PopFrame() {
    lock_guard<mutex> lock(frames_mutex);
    if (frames.empty()) {
        return {};
    }
    auto frame = frames.front();
    frames.pop();
    return frame;
}

unsigned int StorageManager::GetNumberOfFramesInserted() const {
    return frames_count;
}

unsigned int StorageManager::GetNumberOfFramesInQueue() {
    lock_guard<mutex> lock(frames_mutex);
    return frames.size();
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
