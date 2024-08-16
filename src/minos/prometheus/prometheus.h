/*
    Author: Luis Obis (@lobis)
    History: created on 16-Aug-24
*/

#ifndef MCLIENT_PROMETHEUS_H
#define MCLIENT_PROMETHEUS_H

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include <iostream>
#include <mutex>

using namespace prometheus;
using namespace std;

namespace mclient_prometheus {

class PrometheusManager {
public:
    // Static method to access the Singleton instance
    static PrometheusManager& Instance() {
        static PrometheusManager instance;
        return instance;
    }

    // Deleted methods to prevent copy or assignment
    PrometheusManager(const PrometheusManager&) = delete;

    PrometheusManager& operator=(const PrometheusManager&) = delete;

    void SetDaqSpeed(double speed);

    void SetEventId(unsigned int id);

    void SetNumberOfSignalsInEvent(unsigned int number);

private:
    PrometheusManager();

    ~PrometheusManager();

    // Mutex for thread-safe access
    std::mutex mutex_;

    std::unique_ptr<Exposer> exposer;
    std::shared_ptr<Registry> registry;

    Gauge* daq_speed = nullptr;
    Gauge* event_id = nullptr;
    Gauge* number_of_signals_in_event = nullptr; // TODO: histogram
};
} // namespace mclient_prometheus

#endif // MCLIENT_PROMETHEUS_H
