#include <iostream>
#include <memory>

#include "fifo_scheduler.h"
#include "simulation.h"
#include "traffic_generator.h"

int main() {
    auto scheduler = std::make_unique<netsim::FifoScheduler>();
    const double link_rate_mbps = 100.0;
    netsim::Simulation simulation(std::move(scheduler), link_rate_mbps);

    const netsim::TrafficProfile profile{
        30,   // packet_count
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

    std::cout << "Generated " << packets.size()
              << " packets using seed=" << profile.seed
              << " on link_rate=" << link_rate_mbps << " Mb/s\n";

    for (const netsim::Packet& packet : packets) {
        simulation.add_packet(packet);
    }

    simulation.run();

    for (const netsim::Packet& packet : simulation.packets()) {
        std::cout << packet
                  << " arrival=" << packet.arrival_time.count()
                  << "us dequeue=" << packet.dequeue_time.count()
                  << "us departure=" << packet.departure_time.count()
                  << "us wait=" << netsim::wait_time(packet).count()
                  << "us sojourn=" << netsim::sojourn_time(packet).count()
                  << "us\n";
    }
}
