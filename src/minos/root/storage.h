
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

    StorageManager() {
        file = std::make_unique<TFile>("events.root", "RECREATE");
        tree = std::make_unique<TTree>("EventTree", "Tree of DAQ signal events");
        tree->Branch("timestamp", &event.timestamp, "timestamp/L");
        tree->Branch("event_id", &event.id, "event_id/i");
        tree->Branch("signals.id", &event.signal_ids);
        tree->Branch("signals.data", &event.signal_data);
    }

    void Clear() {
        event = {};
    }

    void Checkpoint(bool force = false) {
        constexpr auto time_interval = std::chrono::seconds(10);
        auto now = std::chrono::system_clock::now();
        if (now - lastDrawTime > time_interval) {
            lastDrawTime = now;
            file->Write("", TObject::kOverwrite);
        }
    }

    std::unique_ptr<TFile> file;
    std::unique_ptr<TTree> tree;
    Event event;

    std::chrono::time_point<std::chrono::system_clock> lastDrawTime = std::chrono::system_clock::now();
};

} // namespace mclient_storage

#endif // MCLIENT_STORAGE_H
