
#include "storage.h"

#include <iostream>

using namespace std;
using namespace mclient_storage;

void mclient_storage::StorageManager::Checkpoint(bool force) {
    constexpr auto time_interval = std::chrono::seconds(10);
    auto now = std::chrono::system_clock::now();
    if (force || now - lastDrawTime > time_interval) {
        lastDrawTime = now;
        cout << "Events (N=" << event_tree->GetEntries() << ") have been saved to " << file->GetName() << endl;
        file->Write("", TObject::kOverwrite);
    }
}

StorageManager::StorageManager() {
    file = std::make_unique<TFile>("events.root", "RECREATE");
    // file->SetCompressionAlgorithm(ROOT::kLZMA); // biggest compression ratio but slowest
    // file->SetCompressionLevel(9);               // max compression level

    event_tree = std::make_unique<TTree>("events", "Signal events. Each entry is an event which contains multiple signals");

    event_tree->Branch("timestamp", &event.timestamp, "timestamp/L");
    // tree->Branch("event_id", &event.id, "event_id/i"); It's redundant to store this since it's the same as the entry number
    event_tree->Branch("signal_ids", &event.signal_ids);
    event_tree->Branch("signal_data", &event.signal_data);

    run_tree = std::make_unique<TTree>("run", "Run metadata");

    run_tree->Branch("number", &run_number, "run_number/L");
    run_tree->Branch("timestamp", &timestamp, "timestamp/L");
    run_tree->Branch("name", &run_name);
    run_tree->Branch("comments", &comments);

    // millis since epoch
    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    run_name = "Run " + std::to_string(run_number);

    run_tree->Fill();
}
