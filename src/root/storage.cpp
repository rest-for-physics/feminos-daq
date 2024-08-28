
#include "storage.h"
#include "prometheus.h"
#include <iostream>

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
