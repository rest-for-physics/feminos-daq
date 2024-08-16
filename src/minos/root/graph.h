
#ifndef MCLIENT_GRAPH_H
#define MCLIENT_GRAPH_H

#include <TCanvas.h>
#include <TGraph.h>
#include <iostream>

namespace mclient_graph {
class GraphManager {
public:
    static GraphManager& Instance() {
        static GraphManager instance;
        return instance;
    }

    GraphManager(const GraphManager&) = delete;

    GraphManager& operator=(const GraphManager&) = delete;

    GraphManager() {
        std::cout << "GraphManager::GraphManager()" << std::endl;
        canvas = std::make_unique<TCanvas>("c1", "A Simple Graph Example", 200, 10, 700, 500);

        // graph with 10 points
        graph = std::make_unique<TGraph>();
        for (int i = 0; i < 10; i++) {
            graph->SetPoint(i, i, i * i);
        }

        // show
        canvas->cd();
        graph->Draw("AC*");
        canvas->Update();

        // block
        std::cout << "Press enter to continue" << std::endl;
        std::cin.get();

        std::cout << "GraphManager::GraphManager() done" << std::endl;
    }

    std::unique_ptr<TCanvas> canvas;
    std::unique_ptr<TGraph> graph;
};
} // namespace mclient_graph

#endif // MCLIENT_GRAPH_H
