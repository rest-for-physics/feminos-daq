
#include "prometheus.h"

#include <chrono>
#include <filesystem>
#include <thread>

feminos_daq_prometheus::PrometheusManager::PrometheusManager() {
    exposer = std::make_unique<Exposer>("127.0.0.1:8080");

    registry = std::make_shared<Registry>();

    uptime_seconds = &BuildGauge()
                              .Name("uptime_seconds")
                              .Help("Uptime in seconds (since the start of the program)")
                              .Register(*registry)
                              .Add({});

    auto& free_disk_space_gauge = BuildGauge()
                                          .Name("free_disk_space_gb")
                                          .Help("Free disk space in gigabytes")
                                          .Register(*registry);

    // Gauge to track free disk space
    auto& free_disk_space_metric = free_disk_space_gauge.Add({{"path", "/"}});

    std::thread([this, &free_disk_space_metric]() {
        const auto time_start = std::chrono::steady_clock::now();
        while (true) {
            double free_disk_space_in_gb = GetFreeDiskSpaceGigabytes("/");
            if (free_disk_space_in_gb >= 0) {
                free_disk_space_metric.Set(free_disk_space_in_gb);

                // cout << "Free disk space: " << free_disk_space << " GB" << endl;
                if (free_disk_space_in_gb <= 5.0) {
                    throw std::runtime_error("Free disk space is too low: " + std::to_string(free_disk_space_in_gb) + " GB. Please free up some space.");
                } else if (free_disk_space_in_gb <= 20.0) {
                    std::cerr << "Warning: Free disk space is low: " << free_disk_space_in_gb << " GB" << std::endl;
                }
            }

            const auto millis_uptime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - time_start).count();
            uptime_seconds->Set(double(millis_uptime) / 1000.0);

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }).detach();

    daq_speed_mb_per_s_now = &BuildGauge()
                                      .Name("daq_speed_mb_per_sec_now")
                                      .Help("DAQ speed in megabytes per second")
                                      .Register(*registry)
                                      .Add({});

    daq_speed_mb_per_s = &BuildSummary()
                                  .Name("daq_speed_mb_per_sec")
                                  .Help("DAQ speed in megabytes per second")
                                  .Register(*registry)
                                  .Add({}, Summary::Quantiles{
                                                   {0.01, 0.02},
                                                   {0.1, 0.02},
                                                   {0.25, 0.02},
                                                   {0.5, 0.02},
                                                   {0.75, 0.02},
                                                   {0.9, 0.02},
                                                   {0.99, 0.02},
                                           });

    daq_speed_events_per_s_now = &BuildGauge()
                                          .Name("daq_speed_events_per_sec_now")
                                          .Help("DAQ speed in events per second")
                                          .Register(*registry)
                                          .Add({});

    daq_speed_events_per_s = &BuildSummary()
                                      .Name("daq_speed_events_per_sec")
                                      .Help("DAQ speed in events per second")
                                      .Register(*registry)
                                      .Add({}, Summary::Quantiles{
                                                       {0.01, 0.02},
                                                       {0.1, 0.02},
                                                       {0.25, 0.02},
                                                       {0.5, 0.02},
                                                       {0.75, 0.02},
                                                       {0.9, 0.02},
                                                       {0.99, 0.02},
                                               });

    run_number = &BuildGauge()
                          .Name("run_number")
                          .Help("Run number")
                          .Register(*registry)
                          .Add({});

    number_of_events = &BuildGauge()
                                .Name("number_of_events")
                                .Help("Number of events processed")
                                .Register(*registry)
                                .Add({});

    number_of_signals_in_last_event = &BuildGauge()
                                               .Name("number_of_signals_in_last_event")
                                               .Help("Number of signals in last event")
                                               .Register(*registry)
                                               .Add({});

    number_of_signals_in_event = &BuildSummary()
                                          .Name("number_of_signals_in_event")
                                          .Help("Summary of number of signals per event")
                                          .Register(*registry)
                                          .Add({}, Summary::Quantiles{
                                                           {0.01, 0.02},
                                                           {0.1, 0.02},
                                                           {0.25, 0.02},
                                                           {0.5, 0.02},
                                                           {0.75, 0.02},
                                                           {0.9, 0.02},
                                                           {0.99, 0.02},
                                                   });

    /*
     * Leave this code in case we need a histogram in the future
    {
        auto number_of_signals_in_event_histogram_bucket_boundaries = Histogram::BucketBoundaries{};
        for (int i = 0; i <= 500; i += 25) {
            number_of_signals_in_event_histogram_bucket_boundaries.push_back(i);
        }

        number_of_signals_in_event = &BuildHistogram()
                                                        .Name("number_of_signals_in_event")
                                                        .Help("Histogram of number of signals per event")
                                                        .Register(*registry)
                                                        .Add({}, number_of_signals_in_event_histogram_bucket_boundaries);
    }
    */
    exposer->RegisterCollectable(registry);
}

feminos_daq_prometheus::PrometheusManager::~PrometheusManager() = default;

void feminos_daq_prometheus::PrometheusManager::SetDaqSpeedMB(double speed) {
    if (daq_speed_mb_per_s_now) {
        daq_speed_mb_per_s_now->Set(speed);
    }

    if (daq_speed_mb_per_s) {
        daq_speed_mb_per_s->Observe(speed);
    }
}

void feminos_daq_prometheus::PrometheusManager::SetNumberOfSignalsInEvent(unsigned int number) {
    if (number_of_signals_in_last_event) {
        number_of_signals_in_last_event->Set(number);
    }

    if (number_of_signals_in_event) {
        number_of_signals_in_event->Observe(number);
    }
}

void feminos_daq_prometheus::PrometheusManager::SetNumberOfEvents(unsigned int id) {
    if (number_of_events) {
        number_of_events->Set(id);
    }
}

void feminos_daq_prometheus::PrometheusManager::SetRunNumber(unsigned int id) {
    if (run_number) {
        run_number->Set(id);
    }
}

void feminos_daq_prometheus::PrometheusManager::SetDaqSpeedEvents(double speed) {
    if (daq_speed_events_per_s_now) {
        daq_speed_events_per_s_now->Set(speed);
    }

    if (daq_speed_events_per_s) {
        daq_speed_events_per_s->Observe(speed);
    }
}

void feminos_daq_prometheus::PrometheusManager::ExposeRootOutputFilename(const string& filename) {
    // check file exists and get absolute path
    if (!std::filesystem::exists(filename)) {
        throw std::runtime_error("File does not exist: " + filename);
    }

    auto absolute_path = std::filesystem::absolute(filename).string();

    output_root_filename = absolute_path;
    output_root_file_size = &BuildGauge()
                                     .Name("output_root_file_size_mb")
                                     .Help("Size of the output ROOT file in MB")
                                     .Register(*registry)
                                     .Add({{"filename", output_root_filename}});
}

void feminos_daq_prometheus::PrometheusManager::UpdateOutputRootFileSize() {
    if (output_root_file_size) {
        // check file exists and get size in mb using filesystem
        if (!std::filesystem::exists(output_root_filename)) {
            throw std::runtime_error("File does not exist: " + output_root_filename);
        }

        auto size = std::filesystem::file_size(output_root_filename);
        output_root_file_size->Set(double(size) / (1024 * 1024));
    }
}

double feminos_daq_prometheus::GetFreeDiskSpaceGigabytes(const std::string& path) {
    std::error_code ec;
    std::filesystem::space_info space = std::filesystem::space(path, ec);
    if (ec) {
        // Handle error
        return -1;
    }
    return static_cast<double>(space.free) / (1024 * 1024 * 1024);
}
