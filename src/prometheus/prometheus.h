/*
    Author: Luis Obis (@lobis)
    History: created on 16-Aug-24
*/

#ifndef MCLIENT_PROMETHEUS_H
#define MCLIENT_PROMETHEUS_H

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>

#include <filesystem>
#include <iostream>
#include <mutex>

using namespace prometheus;
using namespace std;

namespace mclient_prometheus {

double GetFreeDiskSpaceGigabytes(const std::string& path = "/");

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

    void SetDaqSpeedMB(double speed);

    void SetDaqSpeedEvents(double speed);

    void SetEventId(unsigned int id);

    void SetNumberOfEvents(unsigned int id);

    void SetRunNumber(unsigned int id);

    void SetNumberOfSignalsInEvent(unsigned int number);

    void ExposeRootOutputFilename(const std::string& filename);

    void UpdateOutputRootFileSize();

private:
    PrometheusManager();

    ~PrometheusManager();

    std::unique_ptr<Exposer> exposer;
    std::shared_ptr<Registry> registry;

    Gauge* number_of_events = nullptr;
    Gauge* daq_speed_mb_per_s = nullptr;
    Gauge* daq_speed_events_per_s = nullptr;
    Gauge* event_id = nullptr;
    Gauge* run_number = nullptr;
    Gauge* number_of_signals_in_event = nullptr;
    Histogram* number_of_signals_in_event_histogram = nullptr;
    Gauge* output_root_file_size = nullptr;
    std::string output_root_filename;
};
} // namespace mclient_prometheus

#endif // MCLIENT_PROMETHEUS_H
