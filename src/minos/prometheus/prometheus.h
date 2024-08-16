/*
    Author: Luis Obis (@lobis)
    History: created on 16-Aug-24
*/

#ifndef MCLIENT_PROMETHEUS_H
#define MCLIENT_PROMETHEUS_H

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>

#include <iostream>
#include <mutex>

using namespace prometheus;
using namespace std;

namespace mclient_prometheus {

    class PrometheusManager {
    public:
        // Static method to access the Singleton instance
        static PrometheusManager &Instance() {
            static PrometheusManager instance;
            return instance;
        }

        // Deleted methods to prevent copy or assignment
        PrometheusManager(const PrometheusManager &) = delete;

        PrometheusManager &operator=(const PrometheusManager &) = delete;

        // Initialize metrics
        void InitializeMetrics();

        // Expose metrics handler
        void ExposeMetrics();


    private:
        PrometheusManager();

        ~PrometheusManager();

        // Mutex for thread-safe access
        std::mutex mutex;

        std::unique_ptr<Exposer> exposer;
        std::shared_ptr<Registry> registry;
    };
}

#endif //MCLIENT_PROMETHEUS_H
