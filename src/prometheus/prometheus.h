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
#include <prometheus/summary.h>
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

    // We cannot expose a string value as a metric directly, so we use a workaround to expose it as a label
    std::string output_root_filename;

    Gauge* number_of_events = nullptr;
    Gauge* daq_speed_mb_per_s_now = nullptr;
    Gauge* daq_speed_events_per_s_now = nullptr;
    Gauge* run_number = nullptr;
    Gauge* number_of_signals_in_last_event = nullptr;
    Summary* number_of_signals_in_event = nullptr;
    Summary* daq_speed_mb_per_s = nullptr;
    Summary* daq_speed_events_per_s = nullptr;
    Gauge* output_root_file_size = nullptr;
};
} // namespace mclient_prometheus

#endif // MCLIENT_PROMETHEUS_H
