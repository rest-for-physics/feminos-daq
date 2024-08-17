
#include "storage.h"

#include <iostream>

using namespace std;
using namespace mclient_storage;

void mclient_storage::StorageManager::Checkpoint(bool force) {
    constexpr auto time_interval = std::chrono::seconds(10);
    auto now = std::chrono::system_clock::now();
    if (force || now - lastDrawTime > time_interval) {
        lastDrawTime = now;
        cout << "Events (N=" << tree->GetEntries() << ") have been saved to " << file->GetName() << endl;
        file->Write("", TObject::kOverwrite);
    }
}
