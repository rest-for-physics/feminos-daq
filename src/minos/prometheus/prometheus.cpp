
#include "prometheus.h"

#include <chrono>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

double GetFreeDiskSpaceGigabytes(const std::string& path) {
    std::error_code ec;
    fs::space_info space = fs::space(path, ec);
    if (ec) {
        // Handle error
        return -1;
    }
    return static_cast<double>(space.free) / (1024 * 1024 * 1024);
}

mclient_prometheus::PrometheusManager::PrometheusManager() {
    exposer = std::make_unique<Exposer>("localhost:8080");

    registry = std::make_shared<Registry>();

    auto& event_counter = BuildCounter()
                                  .Name("events_total")
                                  .Help("Total number of events")
                                  .Register(*registry);

    auto& free_disk_space_gauge = BuildGauge()
                                          .Name("free_disk_space_gb")
                                          .Help("Free disk space in gigabytes")
                                          .Register(*registry);

    // Gauge to track free disk space
    auto& free_disk_space_metric = free_disk_space_gauge.Add({{"path", "/"}});

    std::thread([this, &free_disk_space_metric]() {
        while (true) {
            double free_disk_space = GetFreeDiskSpaceGigabytes("/");
            if (free_disk_space >= 0) {
                free_disk_space_metric.Set(free_disk_space);
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();

    auto& daq_speed_gauge = BuildGauge()
                                    .Name("daq_speed_mb_per_sec")
                                    .Help("DAQ speed in megabytes per second")
                                    .Register(*registry);

    auto& daq_speed_metric = daq_speed_gauge.Add({});

    daq_speed = &daq_speed_metric;

    auto& event_id_gauge = BuildGauge()
                                   .Name("event_id")
                                   .Help("Event ID")
                                   .Register(*registry);

    auto& event_id_metric = event_id_gauge.Add({});

    event_id = &event_id_metric;

    auto& number_of_signals_in_event_gauge = BuildGauge()
                                                     .Name("number_of_signals_in_event")
                                                     .Help("Number of signals in event")
                                                     .Register(*registry);

    auto& number_of_signals_in_event_metric = number_of_signals_in_event_gauge.Add({});

    number_of_signals_in_event = &number_of_signals_in_event_metric;

    exposer->RegisterCollectable(registry);
}

mclient_prometheus::PrometheusManager::~PrometheusManager() = default;

void mclient_prometheus::PrometheusManager::SetDaqSpeed(double speed) {
    lock_guard<mutex> lock(mutex_);

    if (daq_speed) {
        daq_speed->Set(speed);
    }
}

void mclient_prometheus::PrometheusManager::SetEventId(unsigned int id) {
    lock_guard<mutex> lock(mutex_);

    if (event_id) {
        event_id->Set(id);
    }
}

void mclient_prometheus::PrometheusManager::SetNumberOfSignalsInEvent(unsigned int number) {
    lock_guard<mutex> lock(mutex_);

    if (number_of_signals_in_event) {
        number_of_signals_in_event->Set(number);
    }
}
