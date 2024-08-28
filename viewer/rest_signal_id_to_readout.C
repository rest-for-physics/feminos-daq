#include <TRestDetectorReadout.h>
#include <TRestDetectorReadoutModule.h>
#include <TRestDetectorReadoutPlane.h>

#include <fstream>
#include <iostream>
#include <math.h>

using namespace std;

void rest_signal_id_to_readout(TRestDetectorReadout* readout, std::string output_filename = "signal_id_readout_mapping.txt") {
    // write to "output.txt" file
    std::ofstream out(output_filename);

    out << "signal_id_readout_mapping = {\n";

    unsigned int counter = 0;

    for (int i = 0; i < 1000000; i++) {
        const auto signalID = i;

        int readoutChannelID, readoutModuleID, x, y;
        for (int p = 0; p < readout->GetNumberOfReadoutPlanes(); p++) {
            TRestDetectorReadoutPlane* plane = readout->GetReadoutPlane(p);
            const auto planeID = plane->GetID();
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
                        cerr << "Both X and Y are NaN for signalID: " << signalID << " in readout plane: " << planeID << " in readout module: " << readoutModuleID
                             << " in channel: " << readoutChannelID << endl;
                        continue;
                    }
                    bool is_x = false;
                    if (std::isnan(y)) {
                        is_x = true;
                    }
                    auto channel_type = is_x ? "X" : "Y";
                    counter += 1;
                    cout << "- " << counter << " Signal ID: " << signalID << " is in readout plane: " << planeID << " is in readout module: " << readoutModuleID
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
