#include <iostream>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

#include "cbwfq_scheduler.h"
#include "fifo_scheduler.h"
#include "lifo_scheduler.h"
#include "priority_scheduler.h"
#include "simulation.h"
#include "statistics.h"
#include "traffic_generator.h"
#include "weighted_round_robin_scheduler.h"

namespace {
    struct SchedulerRunConfig {
        std::string name;
        bool enabled;
        std::function<std::unique_ptr<netsim::IScheduler>()> create_scheduler;
    };

    struct ScenarioRunConfig {
        std::string name;
        bool enabled;
        netsim::TrafficProfile traffic_profile;
        netsim::DropConfig drop_config;
        double link_rate_mbps;
    };

    netsim::DropConfig make_drop_config(size_t voice_queue_size,
                                        int64_t voice_max_wait_us,
                                        size_t http_queue_size,
                                        int64_t http_max_wait_us,
                                        size_t file_queue_size,
                                        int64_t file_max_wait_us) {
        netsim::DropConfig config;
        config.voice = { voice_queue_size, netsim::SimTime{voice_max_wait_us} };
        config.http  = { http_queue_size,  netsim::SimTime{http_max_wait_us}  };
        config.file  = { file_queue_size,  netsim::SimTime{file_max_wait_us}  };
        return config;
    }

    void initialize_summary_csv(const std::filesystem::path& summary_path) {
        std::ofstream file(summary_path, std::ios::trunc);
        file << "scenario,scheduler,total_packets,transmitted_packets,dropped_packets,"
             << "drop_rate_percent,avg_wait_us,avg_sojourn_us,max_wait_us\n";
    }

    void append_summary_csv(const std::filesystem::path& summary_path,
                            const std::string& scenario_name,
                            const std::string& scheduler_name,
                            const netsim::StatisticsCollector& stats) {
        std::ofstream file(summary_path, std::ios::app);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + summary_path.string());
        }

        const uint32_t transmitted_packets = stats.total_packets_ - stats.total_dropped_;
        file << scenario_name << ","
             << scheduler_name << ","
             << stats.total_packets_ << ","
             << transmitted_packets << ","
             << stats.total_dropped_ << ","
             << std::fixed << std::setprecision(2)
             << stats.overall_drop_rate_percent() << ","
             << std::setprecision(1)
             << stats.overall_avg_wait_time_us() << ","
             << stats.overall_avg_sojourn_time_us() << ","
             << stats.overall_max_wait_time_us() << "\n";
    }

    void write_timeline_csv(const std::filesystem::path& timeline_path,
                            const std::string& scenario_name,
                            const std::string& scheduler_name,
                            const std::vector<netsim::TimelineEntry>& timeline) {
        std::ofstream file(timeline_path, std::ios::trunc);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + timeline_path.string());
        }

        file << "scenario,scheduler,time_us,event_type,packet_id,qos_class,size_bytes,"
             << "queue_voice,queue_http,queue_file,transmitted_so_far,dropped_so_far,"
             << "current_packet_wait_us,avg_wait_so_far_us\n";

        for (const auto& entry : timeline) {
            file << scenario_name << ","
                 << scheduler_name << ","
                 << entry.time.count() << ","
                 << netsim::trace_event_name(entry.event_type) << ","
                 << entry.packet_id << ","
                 << netsim::qos_name(entry.qos_class) << ","
                 << entry.size_bytes << ","
                 << entry.queue_voice << ","
                 << entry.queue_http << ","
                 << entry.queue_file << ","
                 << entry.transmitted_so_far << ","
                 << entry.dropped_so_far << ","
                 << std::fixed << std::setprecision(1)
                 << entry.current_packet_wait_us << ","
                 << entry.avg_wait_so_far_us << "\n";
        }
    }

    void run_scheduler(const ScenarioRunConfig& scenario_config,
                       const SchedulerRunConfig& scheduler_config,
                       const std::vector<netsim::Packet>& packets,
                       const std::filesystem::path& summary_path,
                       const std::filesystem::path& timeline_dir) {
        auto scheduler = scheduler_config.create_scheduler();
        netsim::Simulation simulation(
            std::move(scheduler),
            scenario_config.drop_config,
            scenario_config.link_rate_mbps);

        for (const netsim::Packet& packet : packets) {
            simulation.add_packet(packet);
        }

        simulation.run();

        netsim::StatisticsCollector stats;
        for (const netsim::Packet& packet : simulation.packets()) {
            stats.record(packet);
        }

        std::cout << "\n=== " << scenario_config.name << " / "
                  << scheduler_config.name << " ===\n";
        stats.print_summary();
        append_summary_csv(summary_path, scenario_config.name, scheduler_config.name, stats);

        const auto timeline_path =
            timeline_dir / ("timeline_" + scenario_config.name + "_" + scheduler_config.name + ".csv");
        write_timeline_csv(timeline_path, scenario_config.name, scheduler_config.name, simulation.timeline());
        std::cout << "Timeline written to " << timeline_path.string() << "\n";
    }
}

int main() {
    const std::filesystem::path summary_path = "summary_results.csv";
    const std::filesystem::path timeline_dir = "timeline_results";
    std::filesystem::create_directories(timeline_dir);
    initialize_summary_csv(summary_path);
    const netsim::DropConfig common_drop_config =
        make_drop_config(40, 40'000, 45, 75'000, 50, 110'000);

    const std::vector<SchedulerRunConfig> scheduler_runs{
        {
            "fifo", true,
            [] { return std::make_unique<netsim::FifoScheduler>(); }
        },
        {
            "lifo", true,
            [] { return std::make_unique<netsim::LifoScheduler>(); }
        },
        {
            "priority", true,
            [] { return std::make_unique<netsim::PriorityScheduler>(); }
        },
        {
            "wrr", true,
            [] { return std::make_unique<netsim::WeightedRoundRobinScheduler>(); }
        },
        {
            "cbwfq", true,
            [] { return std::make_unique<netsim::CbwfqScheduler>(); }
        }
    };

    const std::vector<ScenarioRunConfig> scenarios{
        {
            "balanced", true,
            netsim::TrafficProfile{
                180,  // packet_count
                42,   // seed
                350,  // min_interarrival_us
                700,  // max_interarrival_us
                80,   // voice_min_size_bytes
                200,  // voice_max_size_bytes
                500,  // http_min_size_bytes
                1500, // http_max_size_bytes
                800,  // file_min_size_bytes
                1500, // file_max_size_bytes
                35,   // voice_weight
                35,   // http_weight
                30    // file_weight
            },
            common_drop_config,
            20.0
        },
        {
            "medium_load", true,
            netsim::TrafficProfile{
                220,  // packet_count
                52,   // seed
                125,  // min_interarrival_us
                285,  // max_interarrival_us
                80,   // voice_min_size_bytes
                200,  // voice_max_size_bytes
                500,  // http_min_size_bytes
                1500, // http_max_size_bytes
                800,  // file_min_size_bytes
                1500, // file_max_size_bytes
                35,   // voice_weight
                35,   // http_weight
                30    // file_weight
            },
            common_drop_config,
            12.0
        },
        {
            "link_overload", true,
            netsim::TrafficProfile{
                240,  // packet_count
                43,   // seed
                35,   // min_interarrival_us
                100,  // max_interarrival_us
                80,   // voice_min_size_bytes
                200,  // voice_max_size_bytes
                500,  // http_min_size_bytes
                1500, // http_max_size_bytes
                800,  // file_min_size_bytes
                1500, // file_max_size_bytes
                20,   // voice_weight
                60,   // http_weight
                30    // file_weight
            },
            common_drop_config,
            10.0
        },
        {
            "voice_dominance", true,
            netsim::TrafficProfile{
                220,  // packet_count
                44,   // seed
                45,   // min_interarrival_us
                130,  // max_interarrival_us
                80,   // voice_min_size_bytes
                200,  // voice_max_size_bytes
                500,  // http_min_size_bytes
                1500, // http_max_size_bytes
                800,  // file_min_size_bytes
                1500, // file_max_size_bytes
                75,   // voice_weight
                20,   // http_weight
                5     // file_weight
            },
            common_drop_config,
            12.0
        },
        {
            "http_dominance", true,
            netsim::TrafficProfile{
                220,  // packet_count
                46,   // seed
                45,   // min_interarrival_us
                130,  // max_interarrival_us
                80,   // voice_min_size_bytes
                200,  // voice_max_size_bytes
                500,  // http_min_size_bytes
                1500, // http_max_size_bytes
                800,  // file_min_size_bytes
                1500, // file_max_size_bytes
                15,   // voice_weight
                70,   // http_weight
                15    // file_weight
            },
            common_drop_config,
            12.0
        },
        {
            "file_dominance", true,
            netsim::TrafficProfile{
                220,  // packet_count
                45,   // seed
                55,   // min_interarrival_us
                150,  // max_interarrival_us
                80,   // voice_min_size_bytes
                200,  // voice_max_size_bytes
                500,  // http_min_size_bytes
                1500, // http_max_size_bytes
                800,  // file_min_size_bytes
                1500, // file_max_size_bytes
                10,   // voice_weight
                20,   // http_weight
                70    // file_weight
            },
            common_drop_config,
            12.0
        }
    };

    for (const ScenarioRunConfig& scenario_config : scenarios) {
        if (!scenario_config.enabled) {
            continue;
        }

        const netsim::TrafficGenerator generator(scenario_config.traffic_profile);
        const auto packets = generator.generate();

        std::cout << "\n### Scenario: " << scenario_config.name << " ###\n";
        std::cout << "Generated " << packets.size() << " packets\n";
        std::cout << "Link rate: " << scenario_config.link_rate_mbps << " Mbps\n";

        for (const SchedulerRunConfig& scheduler_config : scheduler_runs) {
            if (!scheduler_config.enabled) {
                continue;
            }

            run_scheduler(scenario_config, scheduler_config, packets, summary_path, timeline_dir);
        }
    }

    return 0;
}
