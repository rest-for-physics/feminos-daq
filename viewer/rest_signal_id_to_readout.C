#include <TRestDetectorReadout.h>
#include <TRestDetectorReadoutModule.h>
#include <TRestDetectorReadoutPlane.h>
#include <fstream>
#include <math.h>
#include <iostream>

using namespace std;

void rest_signal_id_to_readout(TRestDetectorReadout* readout, std::string output_filename = "signal_id_readout_mapping.txt") {
    // write to "output.txt" file
    std::ofstream out(output_filename);
    // write header
    out << "# signal_id: ( type (X/Y), position)\n";
    out << "signal_id_readout_mapping = {\n";

    for (int i = 0; i < 100000; i++) {
        auto signalID = i;

        int readoutChannelID, readoutModuleID, x, y;

        for (int p = 0; p < readout->GetNumberOfReadoutPlanes(); p++) {
            TRestDetectorReadoutPlane* plane = readout->GetReadoutPlane(p);
            for (int m = 0; m < plane->GetNumberOfModules(); m++) {
                TRestDetectorReadoutModule* mod = plane->GetModule(m);
                // We iterate over all readout modules searching for the one
                // that contains our signal id
                if (mod->IsDaqIDInside(signalID)) {
                    // If we find it we use the readoutModule id, and the
                    // signalId corresponding
                    // to the physical readout channel and obtain the x,y coordinates.
                    readoutChannelID = mod->DaqToReadoutChannel(signalID);
                    if (readoutChannelID < 0) {
                        continue;
                    }
                    readoutModuleID = mod->GetModuleID();
                    if (readoutModuleID < 0) {
                        continue;
                    }
                    auto x = plane->GetX(readoutModuleID, readoutChannelID);
                    auto y = plane->GetY(readoutModuleID, readoutChannelID);
                    // check if x or y are quiet nan
                    if (std::isnan(x) and std::isnan(y)) {
                        continue;
                    }
                    bool is_x = false;
                    if (std::isnan(y)) {
                        is_x = true;
                    }
                    auto channel_type = is_x ? "X" : "Y";
                    cout << "Signal ID: " << signalID << " is in readout module: " << readoutModuleID
                         << " in channel: " << readoutChannelID << " at " << channel_type << " = "
                         << (is_x ? x : y) << endl;
                    out << "\t" << signalID << ": ( \"" << channel_type << "\", " << (is_x ? x : y) << "),\n";
                    break;
                }
            }
        }
    }
    out << "}\n";
}