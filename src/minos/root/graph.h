
#ifndef MCLIENT_GRAPH_H
#define MCLIENT_GRAPH_H

#include <TApplication.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TMultiGraph.h>

#include <iostream>

#include "storage.h"

namespace mclient_graph {
class GraphManager {
public:
    static GraphManager& Instance() {
        static GraphManager instance;
        return instance;
    }

    GraphManager(const GraphManager&) = delete;

    GraphManager& operator=(const GraphManager&) = delete;

    GraphManager();

    void DrawEvent(const mclient_storage::Event& event);

private:
    std::unique_ptr<TCanvas> canvas;
    std::vector<TGraph> graphs;
    std::unique_ptr<TMultiGraph> multiGraph;
    std::unique_ptr<TApplication> app;
};

} // namespace mclient_graph

#endif // MCLIENT_GRAPH_H
