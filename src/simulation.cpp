#include "simulation.h"

#include <cmath>
#include <stdexcept>

namespace netsim {
    Simulation::Simulation(std::unique_ptr<IScheduler> scheduler, double link_rate_mbps)
        : scheduler_(std::move(scheduler)),
          link_rate_mbps_(link_rate_mbps) {
        if (!scheduler_) {
            throw std::invalid_argument("Simulation requires a scheduler");
        }

        if (link_rate_mbps_ <= 0.0) {
            throw std::invalid_argument("Link rate must be positive");
        }
    }

    void Simulation::add_packet(Packet packet) {
        if (packet_index_.count(packet.id) != 0U) {
            throw std::invalid_argument("Packet id must be unique");
        }

        packet.dropped = false;
        packet.dequeue_time = SimTime::zero();
        packet.departure_time = SimTime::zero();

        packet_index_.emplace(packet.id, packets_.size());
        packets_.push_back(packet);

        schedule_event(Event{packet.arrival_time, EventType::PacketArrival, packet.id});
    }

    void Simulation::run() {
        while (!events_.empty()) {
            process_next_event();
        }
    }

    SimTime Simulation::current_time() const {
        return current_time_;
    }

    const std::vector<Packet>& Simulation::packets() const {
        return packets_;
    }

    void Simulation::schedule_event(const Event& event) {
        events_.push(event);
    }

    void Simulation::process_next_event() {
        const Event event = events_.top();
        events_.pop();

        current_time_ = event.time;

        switch (event.type) {
            case EventType::PacketArrival:
                handle_packet_arrival(event.packet_id);
                break;
            case EventType::TransmissionComplete:
                handle_transmission_complete(event.packet_id);
                break;
        }
    }

    void Simulation::handle_packet_arrival(uint32_t packet_id) {
        scheduler_->enqueue(packet_id);
        try_start_transmission();
    }

    void Simulation::handle_transmission_complete(uint32_t packet_id) {
        Packet& packet = packet_by_id(packet_id);
        packet.departure_time = current_time_;

        transmitter_busy_ = false;
        current_packet_id_.reset();

        try_start_transmission();
    }

    void Simulation::try_start_transmission() {
        if (transmitter_busy_ || !scheduler_->has_packets()) {
            return;
        }

        const uint32_t packet_id = scheduler_->dequeue_next();

        Packet& packet = packet_by_id(packet_id);
        packet.dequeue_time = current_time_;

        transmitter_busy_ = true;
        current_packet_id_ = packet_id;

        schedule_event(Event{
            current_time_ + transmission_time_for(packet),
            EventType::TransmissionComplete,
            packet_id
        });
    }

    Packet& Simulation::packet_by_id(uint32_t packet_id) {
        const auto it = packet_index_.find(packet_id);
        if (it == packet_index_.end()) {
            throw std::out_of_range("Unknown packet id");
        }

        return packets_.at(it->second);
    }

    const Packet& Simulation::packet_by_id(uint32_t packet_id) const {
        const auto it = packet_index_.find(packet_id);
        if (it == packet_index_.end()) {
            throw std::out_of_range("Unknown packet id");
        }

        return packets_.at(it->second);
    }

    SimTime Simulation::transmission_time_for(const Packet& packet) const {
        // 1 Mb/s equals 1 bit/us, so the configured rate can be used as bits per microsecond.
        const double bits_to_send = static_cast<double>(packet.size_bytes) * 8.0;
        const double duration_us = bits_to_send / link_rate_mbps_;
        const auto rounded_up_us = static_cast<int64_t>(std::ceil(duration_us));
        return SimTime{rounded_up_us > 0 ? rounded_up_us : 1};
    }
}
