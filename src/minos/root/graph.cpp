
#include "graph.h"

#include <TAxis.h>
#include <TText.h>
#include <TThread.h>

using namespace mclient_graph;

GraphManager::GraphManager() {
    int argc = 0;
    char** argv = nullptr;

    app = std::make_unique<TApplication>("app", &argc, argv);

    canvas = std::make_unique<TCanvas>("canvas", "mclient", 1600, 900);

    int nGraphs = 5;
    constexpr int n = 512;

    graphs.clear();
    for (int i = 0; i < nGraphs; i++) {
        graphs.emplace_back();
        auto& graph = graphs.back();
        graph.SetTitle(Form("Graph %d", i));

        for (int j = 0; j < n; j++) {
            graph.SetPoint(j, j, sin(j / (10.0 * (i + 1))) * 100);
        }

        graph.SetLineWidth(3);
        // random color
        graph.SetLineColor(i + 1);
    }

    multiGraph = std::make_unique<TMultiGraph>("multiGraph", "mclient");

    for (auto& graph: graphs) {
        multiGraph->Add(&graph);
    }

    multiGraph->GetXaxis()->SetTitle("Time bin");
    multiGraph->GetYaxis()->SetTitle("ADC value");
    multiGraph->GetXaxis()->SetRangeUser(0, n);
    // multiGraph->GetXaxis()->SetNdivisions(n / 64, 5, 0);

    // draw
    multiGraph->Draw("AL");

    // TODO: correct labels (instead of every 100, every 128 points, etc)

    app->Run();
}
