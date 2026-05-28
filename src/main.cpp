#include <iostream>
#include <cstdint>
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

    void run_scheduler(const ScenarioRunConfig& scenario_config,
                       const SchedulerRunConfig& scheduler_config,
                       const std::vector<netsim::Packet>& packets) {
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

        const std::string csv_name =
            "results_" + scenario_config.name + "_" + scheduler_config.name + ".csv";
        stats.write_csv(csv_name);
        std::cout << "Results written to " << csv_name << "\n";
    }
}

int main() {
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
            make_drop_config(60, 70'000, 70, 120'000, 90, 0),
            20.0
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
            make_drop_config(35, 50'000, 45, 70'000, 45, 90'000),
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
            make_drop_config(45, 35'000, 35, 80'000, 30, 120'000),
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
            make_drop_config(35, 45'000, 40, 90'000, 60, 130'000),
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

            run_scheduler(scenario_config, scheduler_config, packets);
        }
    }

    return 0;
}
