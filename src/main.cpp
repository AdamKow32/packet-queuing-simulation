#include <iostream>
#include <memory>

#include "fifo_scheduler.h"
#include "simulation.h"
#include "statistics.h"
#include "traffic_generator.h"

int main() {
    const double link_rate_mbps = 100.0;
    netsim::DropConfig drop_config;
    drop_config.voice = { 10,  netsim::SimTime{150'000} };
    drop_config.http  = { 50,  netsim::SimTime{500'000} };
    drop_config.file  = { 100, netsim::SimTime{0}       };

    auto scheduler = std::make_unique<netsim::FifoScheduler>();
    netsim::Simulation simulation(std::move(scheduler), drop_config, link_rate_mbps);

    const netsim::TrafficProfile profile{
        100,  // packet_count
        42,   // seed
        100,  // min_interarrival_us
        800,  // max_interarrival_us
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

    for (const netsim::Packet& packet : packets) {
        simulation.add_packet(packet);
    }

    simulation.run();
    netsim::StatisticsCollector stats;
    for (const netsim::Packet& packet : simulation.packets()) {
        stats.record(packet);
    }

    stats.print_summary();
    stats.write_csv("results.csv");
    std::cout << "Results written to results.csv\n";

    return 0;
}