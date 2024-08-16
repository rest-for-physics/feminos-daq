
#include "prometheus.h"

#include <thread>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

double GetFreeDiskSpaceMegabytes(const std::string &path) {
    std::error_code ec;
    fs::space_info space = fs::space(path, ec);
    if (ec) {
        // Handle error
        return -1;
    }
    return static_cast<double>(space.free) / (1024 * 1024); // Convert bytes to megabytes
}

void mclient_prometheus::PrometheusManager::InitializeMetrics() {

}

void mclient_prometheus::PrometheusManager::ExposeMetrics() {

}

mclient_prometheus::PrometheusManager::PrometheusManager() {
    cout << "PrometheusManager constructor" << endl;
    exposer = std::make_unique<Exposer>("localhost:8080");

    registry = std::make_shared<Registry>();

    auto &event_counter = BuildCounter()
            .Name("events_total")
            .Help("Total number of events")
            .Register(*registry);

    auto &free_disk_space_gauge = BuildGauge()
            .Name("free_disk_space_mb")
            .Help("Free disk space in megabytes")
            .Register(*registry);

    // Gauge to track free disk space
    auto &free_disk_space_metric = free_disk_space_gauge.Add({{"path", "/"}});

    // Start a thread to update the free disk space metric every 10 seconds
    std::thread([this, &free_disk_space_metric]() {
        while (true) {
            double free_disk_space = GetFreeDiskSpaceMegabytes("/");
            if (free_disk_space >= 0) {
                free_disk_space_metric.Set(free_disk_space);
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();

    auto &daq_speed_gauge = BuildGauge()
            .Name("daq_speed_mb_per_sec")
            .Help("DAQ speed in megabytes per second")
            .Register(*registry);

    auto &daq_speed_metric = daq_speed_gauge.Add({});

    daq_speed = &daq_speed_metric;

    // Start the metrics server
    exposer->RegisterCollectable(registry);
}

mclient_prometheus::PrometheusManager::~PrometheusManager() = default;

void mclient_prometheus::PrometheusManager::SetDaqSpeed(double speed) {
    lock_guard<mutex> lock(mutex_);

    if (daq_speed) {
        daq_speed->Set(speed);
    }
}
