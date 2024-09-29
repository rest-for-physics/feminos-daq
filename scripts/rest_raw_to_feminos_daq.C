#include <TFile.h>
#include <TRestRawSignalEvent.h>
#include <TRestRun.h>

#include <iostream>
#include <string>
#include <vector>

using namespace std;

/// Input file: A REST file containing a TRestRawSignalEvent
/// Output file: A root file with signal data stored in a compatible format with feminos-daq
void rest_raw_to_feminos_daq(const std::string& input_filename, const std::string& output_filename) {
    auto input_file = TFile::Open(input_filename.c_str(), "READ");
    auto input_tree = input_file->Get<TTree>("EventTree");

    TRestRawSignalEvent* event = nullptr;

    input_tree->SetBranchAddress("TRestRawSignalEventBranch", &event);

    auto output_file = TFile::Open(output_filename.c_str(), "RECREATE");
    auto output_tree = TTree("events", "events");

    unsigned long long timestamp = 0;
    std::vector<unsigned short> signal_ids;
    std::vector<unsigned short> signal_values;

    output_tree.Branch("timestamp", &timestamp);
    output_tree.Branch("signal_ids", &signal_ids);
    output_tree.Branch("signal_values", &signal_values);

    for (int i = 0; i < input_tree->GetEntries(); i++) {
        signal_ids.clear();
        signal_values.clear();

        input_tree->GetEntry(i);

        for (int j = 0; j < event->GetNumberOfSignals(); j++) {
            TRestRawSignal* signal = event->GetSignal(j);
            signal_ids.push_back(signal->GetSignalID());
            for (int k = 0; k < signal->GetNumberOfPoints(); k++) {
                signal_values.push_back(signal->GetData(k));
            }
        }

        // make sure the length of signal_data is 512 the length of signal_ids
        if (signal_values.size() != 512 * signal_ids.size()) {
            std::cerr << "Error: signal_values.size() != 512 * signal_ids.size()" << std::endl;
            return;
        }

        output_tree.Fill();
    }

    output_file->Write();
    output_file->Close();
}
