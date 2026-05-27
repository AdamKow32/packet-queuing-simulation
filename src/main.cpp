#include <iostream>
#include <functional>
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

    void run_scheduler(const SchedulerRunConfig& scheduler_config,
                       const std::vector<netsim::Packet>& packets,
                       const netsim::DropConfig& drop_config,
                       double link_rate_mbps) {
        auto scheduler = scheduler_config.create_scheduler();
        netsim::Simulation simulation(std::move(scheduler), drop_config, link_rate_mbps);

        for (const netsim::Packet& packet : packets) {
            simulation.add_packet(packet);
        }

        simulation.run();

        netsim::StatisticsCollector stats;
        for (const netsim::Packet& packet : simulation.packets()) {
            stats.record(packet);
        }

        std::cout << "\n=== " << scheduler_config.name << " ===\n";
        stats.print_summary();

        const std::string csv_name = "results_" + scheduler_config.name + ".csv";
        stats.write_csv(csv_name);
        std::cout << "Results written to " << csv_name << "\n";
    }
}

int main() {
    const double link_rate_mbps = 10.0;
    netsim::DropConfig drop_config;
    drop_config.voice = { 35, netsim::SimTime{50'000} };
    drop_config.http  = { 45, netsim::SimTime{70'000} };
    drop_config.file  = { 45, netsim::SimTime{90'000} };

    const netsim::TrafficProfile profile{
        180,  // packet_count
        42,   // seed
        45,   // min_interarrival_us
        140,  // max_interarrival_us
        80,   // voice_min_size_bytes
        200,  // voice_max_size_bytes
        500,  // http_min_size_bytes
        1500, // http_max_size_bytes
        800,  // file_min_size_bytes
        1500, // file_max_size_bytes
        20,   // voice_weight
        60,   // http_weight
        30    // file_weight
    };

    const netsim::TrafficGenerator generator(profile);
    const auto packets = generator.generate();

    std::cout << "Generated " << packets.size() << " packets\n";

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

    for (const SchedulerRunConfig& scheduler_config : scheduler_runs) {
        if (!scheduler_config.enabled) {
            continue;
        }

        run_scheduler(scheduler_config, packets, drop_config, link_rate_mbps);
    }

    return 0;
}
