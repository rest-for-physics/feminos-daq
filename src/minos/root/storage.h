
#ifndef MCLIENT_STORAGE_H
#define MCLIENT_STORAGE_H

#include <TFile.h>
#include <TTree.h>
#include <array>
#include <string>
#include <vector>

namespace mclient_storage {

class Event {
public:
    constexpr static size_t SIGNAL_SIZE = 512;
    unsigned long long timestamp = 0;
    unsigned int id = 0;
    std::vector<unsigned short> signal_ids;
    std::vector<unsigned short> signal_data; // all data points from all signals concatenated (same order as signal_ids)

    size_t size() const {
        return signal_ids.size();
    }

    std::pair<unsigned short, std::array<unsigned short, SIGNAL_SIZE>> get_signal_id_data_pair(size_t index) const {
        unsigned short channel = signal_ids[index];
        std::array<unsigned short, SIGNAL_SIZE> data{};
        for (size_t i = 0; i < SIGNAL_SIZE; ++i) {
            data[i] = signal_data[index * 512 + i];
        }
        return {channel, data};
    }

    void add_signal(unsigned short id, const std::array<unsigned short, SIGNAL_SIZE>& data) {
        signal_ids.push_back(id);
        for (size_t i = 0; i < SIGNAL_SIZE; ++i) {
            signal_data.push_back(data[i]);
        }
    }
};

class StorageManager {
public:
    static StorageManager& Instance() {
        static StorageManager instance;
        return instance;
    }

    StorageManager(const StorageManager&) = delete;

    StorageManager& operator=(const StorageManager&) = delete;

    StorageManager();

    void Clear() {
        event = {};
    }

    Long64_t GetNumberOfEvents() const {
        return event_tree->GetEntries();
    }

    void Checkpoint(bool force = false);

    std::unique_ptr<TFile> file;
    std::unique_ptr<TTree> event_tree;
    std::unique_ptr<TTree> run_tree;
    Event event;

    unsigned long long run_number = 0;
    unsigned long long run_time_start = 0;
    std::string run_name;
    std::string comments;

    unsigned long long millisSinceEpochForSpeedCalculation = 0;

    double GetSpeedEventsPerSecond() const;

private:
    std::chrono::time_point<std::chrono::system_clock> lastCheckpointTime = std::chrono::system_clock::now();
};

} // namespace mclient_storage

#endif // MCLIENT_STORAGE_H
