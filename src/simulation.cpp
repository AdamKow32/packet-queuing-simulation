#include "simulation.h"

#include <cmath>
#include <stdexcept>

namespace netsim {
    const char* trace_event_name(TraceEventType type) {
        switch (type) {
            case TraceEventType::ArrivalAccepted:
                return "arrival_accepted";
            case TraceEventType::DropQueueLimit:
                return "drop_queue_limit";
            case TraceEventType::TransmissionStart:
                return "transmission_start";
            case TraceEventType::TransmissionComplete:
                return "transmission_complete";
            case TraceEventType::DropWaitTimeout:
                return "drop_wait_timeout";
            default:
                return "unknown";
        }
    }

    Simulation::Simulation(std::unique_ptr<IScheduler> scheduler,
                           DropConfig                  drop_config,
                           double                      link_rate_mbps)
        : scheduler_(std::move(scheduler))
        , drop_config_(drop_config)
        , link_rate_mbps_(link_rate_mbps) {
        if (!scheduler_) {
            throw std::invalid_argument("Simulation requires a scheduler");
        }
        if (link_rate_mbps_ <= 0.0) {
            throw std::invalid_argument("Link rate must be positive");
        }
        queue_sizes_.fill(0);
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

    const std::vector<TimelineEntry>& Simulation::timeline() const {
        return timeline_;
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
        Packet& packet = packet_by_id(packet_id);
        const ClassDropConfig& cfg = drop_config_.for_class(packet.qos_class);
        const size_t cls_index = static_cast<size_t>(packet.qos_class);

        if (cfg.max_queue_size > 0 &&
            queue_sizes_[cls_index] >= cfg.max_queue_size) {
            packet.dropped = true;
            dropped_so_far_++;
            record_timeline_event(TraceEventType::DropQueueLimit, packet);
            return;
        }

        scheduler_->enqueue(packet_id, packet.qos_class, packet.size_bytes);
        queue_sizes_[cls_index]++;
        record_timeline_event(TraceEventType::ArrivalAccepted, packet);

        try_start_transmission();
    }

    void Simulation::handle_transmission_complete(uint32_t packet_id) {
        Packet& packet = packet_by_id(packet_id);
        packet.departure_time = current_time_;
        transmitted_so_far_++;
        sum_wait_time_us_completed_ += static_cast<double>(wait_time(packet).count());
        record_timeline_event(TraceEventType::TransmissionComplete, packet, static_cast<double>(wait_time(packet).count()));

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
        const size_t cls_index = static_cast<size_t>(packet.qos_class);
        const ClassDropConfig& cfg = drop_config_.for_class(packet.qos_class);

        queue_sizes_[cls_index]--;

        if (cfg.max_wait_time > SimTime{0}) {
            const SimTime wait = current_time_ - packet.arrival_time;
            if (wait > cfg.max_wait_time) {
                packet.dropped = true;
                dropped_so_far_++;
                record_timeline_event(TraceEventType::DropWaitTimeout, packet, static_cast<double>(wait.count()));
                try_start_transmission();
                return;
            }
        }

        packet.dequeue_time = current_time_;
        transmitter_busy_ = true;
        current_packet_id_ = packet_id;
        record_timeline_event(TraceEventType::TransmissionStart, packet, static_cast<double>(wait_time(packet).count()));

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
        const double bits_to_send = static_cast<double>(packet.size_bytes) * 8.0;
        const double duration_us = bits_to_send / link_rate_mbps_;
        const auto rounded_up_us = static_cast<int64_t>(std::ceil(duration_us));
        return SimTime{rounded_up_us > 0 ? rounded_up_us : 1};
    }

    void Simulation::record_timeline_event(TraceEventType event_type,
                                           const Packet& packet,
                                           double current_packet_wait_us) {
        timeline_.push_back(TimelineEntry{
            current_time_,
            event_type,
            packet.id,
            packet.qos_class,
            packet.size_bytes,
            queue_sizes_[static_cast<std::size_t>(QoSClass::Voice)],
            queue_sizes_[static_cast<std::size_t>(QoSClass::HTTP)],
            queue_sizes_[static_cast<std::size_t>(QoSClass::File)],
            transmitted_so_far_,
            dropped_so_far_,
            current_packet_wait_us,
            avg_wait_so_far_us()
        });
    }

    double Simulation::avg_wait_so_far_us() const {
        if (transmitted_so_far_ == 0U) {
            return 0.0;
        }

        return sum_wait_time_us_completed_ / static_cast<double>(transmitted_so_far_);
    }

}
