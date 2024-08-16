
#include "graph.h"

#include <TAxis.h>
#include <TLegend.h>
#include <TText.h>
#include <TThread.h>
#include <TSystem.h>

#include <iostream>

using namespace mclient_graph;
using namespace std;

GraphManager::GraphManager() {
    int argc = 0;
    char** argv = nullptr;

    app = std::make_unique<TApplication>("app", &argc, argv);

    canvas = std::make_unique<TCanvas>("canvas", "mclient", 1600, 900);

    multiGraph = std::make_unique<TMultiGraph>("multiGraph", "mclient");

    multiGraph->GetXaxis()->SetTitle("Time bin");
    multiGraph->GetYaxis()->SetTitle("ADC value");
}

void GraphManager::DrawEvent(const mclient_storage::Event& event) {
    graphs.clear();
    for (size_t i = 0; i < event.size(); i++) {
        auto [channel, data] = event.get_signal_id_data_pair(i);
        graphs.emplace_back();
        auto& graph = graphs.back();
        graph.SetTitle(Form("Channel %d", channel));

        for (size_t j = 0; j < data.size(); j++) {
            cout << "j: " << j << " data[j]: " << data[j] << endl;
            graph.SetPoint(j, j, data[j]);
        }

        graph.SetLineWidth(3);
        // random color
        graph.SetLineColor(i + 1);
    }

    // update draw
    multiGraph->Clear();
    for (auto& graph: graphs) {
        multiGraph->Add(&graph);
    }

    multiGraph->GetXaxis()->SetRangeUser(0, mclient_storage::Event::SIGNAL_SIZE);
    multiGraph->GetXaxis()->SetNdivisions(mclient_storage::Event::SIGNAL_SIZE / 64, 5, 0);

    canvas->cd();
    multiGraph->Draw("AL");

    canvas->Update();
    canvas->Pad()->Draw();

    gSystem->ProcessEvents();

    app->Run();
}
