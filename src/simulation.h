#pragma once

#include <cstdint>
#include <optional>
#include <queue>
#include <memory>
#include <unordered_map>
#include <vector>

#include "event.h"
#include "packet.h"
#include "scheduler.h"

namespace netsim {
    class Simulation {
    public:
        explicit Simulation(std::unique_ptr<IScheduler> scheduler, double link_rate_mbps = 100.0);

        void add_packet(Packet packet);
        void run();

        SimTime current_time() const;
        const std::vector<Packet>& packets() const;

    private:
        void schedule_event(const Event& event);
        void process_next_event();
        void handle_packet_arrival(uint32_t packet_id);
        void handle_transmission_complete(uint32_t packet_id);
        void try_start_transmission();
        SimTime transmission_time_for(const Packet& packet) const;

        Packet& packet_by_id(uint32_t packet_id);
        const Packet& packet_by_id(uint32_t packet_id) const;

        SimTime current_time_{SimTime::zero()};
        std::priority_queue<Event, std::vector<Event>, EventCompare> events_;
        std::vector<Packet> packets_;
        std::unordered_map<uint32_t, std::size_t> packet_index_;
        std::unique_ptr<IScheduler> scheduler_;
        double link_rate_mbps_{100.0};
        bool transmitter_busy_{false};
        std::optional<uint32_t> current_packet_id_{};
    };
}
