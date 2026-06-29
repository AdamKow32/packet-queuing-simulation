#include "simulation.h"

#include <algorithm>
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
                           DropConfig drop_config,
                           double link_rate_mbps)
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
        generated_by_class_so_far_.fill(0);
        dropped_by_class_so_far_.fill(0);
        transmitted_by_class_so_far_.fill(0);
        sum_wait_by_class_so_far_.fill(0.0);
        max_wait_by_class_so_far_.fill(0.0);
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

        schedule_event(Event{
            packet.arrival_time,
            EventType::PacketArrival,
            packet.id
        });
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

        generated_so_far_++;
        generated_by_class_so_far_[cls_index]++;

        if (cfg.max_queue_size > 0 &&
            queue_sizes_[cls_index] >= cfg.max_queue_size) {
            packet.dropped = true;
            dropped_so_far_++;
            dropped_by_class_so_far_[cls_index]++;
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

        const std::size_t cls_index =
            static_cast<std::size_t>(packet.qos_class);

        const double completed_wait_us =
            static_cast<double>(wait_time(packet).count());

        transmitted_so_far_++;
        transmitted_by_class_so_far_[cls_index]++;

        sum_wait_time_us_completed_ += completed_wait_us;
        sum_wait_by_class_so_far_[cls_index] += completed_wait_us;

        if (completed_wait_us > max_wait_time_us_completed_) {
            max_wait_time_us_completed_ = completed_wait_us;
        }

        if (completed_wait_us > max_wait_by_class_so_far_[cls_index]) {
            max_wait_by_class_so_far_[cls_index] = completed_wait_us;
        }

        record_timeline_event(
            TraceEventType::TransmissionComplete,
            packet,
            completed_wait_us
        );

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
                dropped_by_class_so_far_[cls_index]++;

                record_timeline_event(
                    TraceEventType::DropWaitTimeout,
                    packet,
                    static_cast<double>(wait.count())
                );

                try_start_transmission();
                return;
            }
        }

        packet.dequeue_time = current_time_;

        transmitter_busy_ = true;
        current_packet_id_ = packet_id;

        record_timeline_event(
            TraceEventType::TransmissionStart,
            packet,
            static_cast<double>(wait_time(packet).count())
        );

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
        const double bits_to_send =
            static_cast<double>(packet.size_bytes) * 8.0;

        const double duration_us =
            bits_to_send / link_rate_mbps_;

        const auto rounded_up_us =
            static_cast<int64_t>(std::ceil(duration_us));

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
            generated_so_far_,

            current_packet_wait_us,
            avg_wait_so_far_us(),
            max_wait_time_us_completed_,
            drop_rate_so_far_percent(),
            fairness_so_far(),
            objective_score_so_far()
        });
    }

    double Simulation::avg_wait_so_far_us() const {
        if (transmitted_so_far_ == 0U) {
            return 0.0;
        }

        return sum_wait_time_us_completed_
             / static_cast<double>(transmitted_so_far_);
    }

    double Simulation::drop_rate_so_far_percent() const {
        if (generated_so_far_ == 0U) {
            return 0.0;
        }

        return 100.0 * static_cast<double>(dropped_so_far_)
                     / static_cast<double>(generated_so_far_);
    }

    double Simulation::fairness_so_far() const {
        constexpr std::array<double, NUM_QOS_CLASSES> class_weights{
            4.0,
            2.0,
            1.0
        };

        std::array<double, NUM_QOS_CLASSES> demand{};
        std::array<double, NUM_QOS_CLASSES> served{};
        std::array<double, NUM_QOS_CLASSES> ideal{};
        std::array<bool, NUM_QOS_CLASSES> active{};

        double total_served = 0.0;

        for (std::size_t i = 0; i < NUM_QOS_CLASSES; ++i) {
            demand[i] =
                static_cast<double>(generated_by_class_so_far_[i]);

            served[i] =
                static_cast<double>(transmitted_by_class_so_far_[i]);

            total_served += served[i];

            if (demand[i] > 0.0) {
                active[i] = true;
            }
        }

        if (total_served == 0.0) {
            return 0.0;
        }

        double remaining_capacity = total_served;

        while (remaining_capacity > 1e-9) {
            double active_weight_sum = 0.0;
            std::size_t active_count = 0;

            for (std::size_t i = 0; i < NUM_QOS_CLASSES; ++i) {
                if (active[i]) {
                    active_weight_sum += class_weights[i];
                    active_count++;
                }
            }

            if (active_count == 0 || active_weight_sum == 0.0) {
                break;
            }

            const double round_capacity = remaining_capacity;
            bool capped_any_class = false;

            for (std::size_t i = 0; i < NUM_QOS_CLASSES; ++i) {
                if (!active[i]) {
                    continue;
                }

                const double weighted_allocation =
                    round_capacity * class_weights[i] / active_weight_sum;

                if (weighted_allocation >= demand[i]) {
                    ideal[i] = demand[i];
                    remaining_capacity -= demand[i];
                    active[i] = false;
                    capped_any_class = true;
                }
            }

            if (!capped_any_class) {
                for (std::size_t i = 0; i < NUM_QOS_CLASSES; ++i) {
                    if (active[i]) {
                        ideal[i] =
                            remaining_capacity * class_weights[i] / active_weight_sum;
                    }
                }

                remaining_capacity = 0.0;
            }
        }

        double sum = 0.0;
        double sum_squares = 0.0;
        double active_classes = 0.0;

        for (std::size_t i = 0; i < NUM_QOS_CLASSES; ++i) {
            if (ideal[i] <= 0.0) {
                continue;
            }

            const double ratio = served[i] / ideal[i];

            sum += ratio;
            sum_squares += ratio * ratio;
            active_classes += 1.0;
        }

        if (active_classes == 0.0 || sum_squares == 0.0) {
            return 0.0;
        }

        return (sum * sum) / (active_classes * sum_squares);
    }

    double Simulation::objective_score_so_far() const {
        constexpr double global_score_weight = 0.70;
        constexpr double class_score_weight = 0.20;
        constexpr double priority_penalty_weight = 0.10;

        constexpr double wait_weight = 0.25;
        constexpr double loss_weight = 0.45;
        constexpr double max_wait_weight = 0.20;
        constexpr double unfairness_weight = 0.10;

        constexpr double reference_wait_us = 100'000.0;

        const double normalized_wait =
            avg_wait_so_far_us() / reference_wait_us;

        const double normalized_loss =
            drop_rate_so_far_percent() / 100.0;

        const double normalized_max_wait =
            max_wait_time_us_completed_ / reference_wait_us;

        const double unfairness =
            1.0 - fairness_so_far();

        const double global_score =
            wait_weight * normalized_wait +
            loss_weight * normalized_loss +
            max_wait_weight * normalized_max_wait +
            unfairness_weight * unfairness;

        constexpr double voice_wait_limit_us = 20'000.0;
        constexpr double http_wait_limit_us = 80'000.0;
        constexpr double file_wait_limit_us = 300'000.0;

        const double voice_cost =
            class_cost_so_far(QoSClass::Voice, voice_wait_limit_us);

        const double http_cost =
            class_cost_so_far(QoSClass::HTTP, http_wait_limit_us);

        const double file_cost =
            class_cost_so_far(QoSClass::File, file_wait_limit_us);

        const double class_priority_score =
            0.50 * voice_cost +
            0.30 * http_cost +
            0.20 * file_cost;

        const double priority_violation_penalty =
            (
                std::max(0.0, voice_cost - http_cost) +
                std::max(0.0, voice_cost - file_cost) +
                0.5 * std::max(0.0, http_cost - file_cost)
            ) / 2.5;

        return global_score_weight * global_score +
               class_score_weight * class_priority_score +
               priority_penalty_weight * priority_violation_penalty;
    }

    double Simulation::class_cost_so_far(QoSClass qos_class,
                                         double wait_limit_us) const {
        constexpr double class_wait_weight = 0.30;
        constexpr double class_loss_weight = 0.50;
        constexpr double class_max_wait_weight = 0.20;

        const std::size_t cls_index =
            static_cast<std::size_t>(qos_class);

        const double generated =
            static_cast<double>(generated_by_class_so_far_[cls_index]);

        const double dropped =
            static_cast<double>(dropped_by_class_so_far_[cls_index]);

        const double transmitted =
            static_cast<double>(transmitted_by_class_so_far_[cls_index]);

        const double avg_wait =
            transmitted > 0.0
                ? sum_wait_by_class_so_far_[cls_index] / transmitted
                : 0.0;

        const double drop_rate =
            generated > 0.0
                ? dropped / generated
                : 0.0;

        const double normalized_wait =
            std::min(1.0, avg_wait / wait_limit_us);

        const double normalized_max_wait =
            std::min(1.0, max_wait_by_class_so_far_[cls_index] / wait_limit_us);

        return class_wait_weight * normalized_wait +
               class_loss_weight * drop_rate +
               class_max_wait_weight * normalized_max_wait;
    }
}