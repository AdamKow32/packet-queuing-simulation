#pragma once

#include <cstdint>
#include <optional>
#include <queue>
#include <memory>
#include <unordered_map>
#include <vector>
#include <array>

#include "event.h"
#include "packet.h"
#include "scheduler.h"

namespace netsim {
    enum class TraceEventType : uint8_t {
        ArrivalAccepted = 0,
        DropQueueLimit = 1,
        TransmissionStart = 2,
        TransmissionComplete = 3,
        DropWaitTimeout = 4
    };

    struct TimelineEntry {
        SimTime time{};
        TraceEventType event_type{TraceEventType::ArrivalAccepted};
        uint32_t packet_id{0};
        QoSClass qos_class{QoSClass::HTTP};
        uint32_t size_bytes{0};
        size_t queue_voice{0};
        size_t queue_http{0};
        size_t queue_file{0};
        uint32_t transmitted_so_far{0};
        uint32_t dropped_so_far{0};
        uint32_t generated_so_far{0};
        double current_packet_wait_us{0.0};
        double avg_wait_so_far_us{0.0};
        double max_wait_so_far_us{0.0};
        double drop_rate_so_far_percent{0.0};
        double fairness_so_far{0.0};
        double objective_score_so_far{0.0};
    };

    const char* trace_event_name(TraceEventType type);

    struct ClassDropConfig {
        size_t  max_queue_size;
        SimTime max_wait_time;
    };

    struct DropConfig {
        ClassDropConfig voice { 10,  SimTime{150'000} };
        ClassDropConfig http  { 50,  SimTime{500'000} };
        ClassDropConfig file  { 100, SimTime{0}       };

        const ClassDropConfig& for_class(QoSClass cls) const {
            switch (cls) {
                case QoSClass::Voice: return voice;
                case QoSClass::HTTP:  return http;
                case QoSClass::File:  return file;
                default:              return file;
            }
        }
    };

    class Simulation {
    public:
        explicit Simulation(
            std::unique_ptr<IScheduler> scheduler,
            DropConfig drop_config = {},
            double link_rate_mbps = 100.0
        );

        void add_packet(Packet packet);
        void run();

        SimTime current_time() const;
        const std::vector<Packet>& packets() const;
        const std::vector<TimelineEntry>& timeline() const;

    private:
        void schedule_event(const Event& event);
        void process_next_event();
        void handle_packet_arrival(uint32_t packet_id);
        void handle_transmission_complete(uint32_t packet_id);
        void try_start_transmission();
        SimTime transmission_time_for(const Packet& packet) const;

        void record_timeline_event(
            TraceEventType event_type,
            const Packet& packet,
            double current_packet_wait_us = 0.0
        );

        double avg_wait_so_far_us() const;
        double drop_rate_so_far_percent() const;
        double fairness_so_far() const;
        double objective_score_so_far() const;
        double class_cost_so_far(QoSClass qos_class, double wait_limit_us) const;

        Packet& packet_by_id(uint32_t packet_id);
        const Packet& packet_by_id(uint32_t packet_id) const;

        std::array<size_t, NUM_QOS_CLASSES> queue_sizes_{};

        SimTime current_time_{SimTime::zero()};
        std::priority_queue<Event, std::vector<Event>, EventCompare> events_;
        std::vector<Packet> packets_;
        std::vector<TimelineEntry> timeline_;
        std::unordered_map<uint32_t, std::size_t> packet_index_;
        std::unique_ptr<IScheduler> scheduler_;
        DropConfig drop_config_;
        double link_rate_mbps_{100.0};
        bool transmitter_busy_{false};
        std::optional<uint32_t> current_packet_id_{};

        uint32_t transmitted_so_far_{0};
        uint32_t dropped_so_far_{0};
        uint32_t generated_so_far_{0};

        std::array<uint32_t, NUM_QOS_CLASSES> generated_by_class_so_far_{};
        std::array<uint32_t, NUM_QOS_CLASSES> dropped_by_class_so_far_{};
        std::array<uint32_t, NUM_QOS_CLASSES> transmitted_by_class_so_far_{};

        double sum_wait_time_us_completed_{0.0};
        double max_wait_time_us_completed_{0.0};

        std::array<double, NUM_QOS_CLASSES> sum_wait_by_class_so_far_{};
        std::array<double, NUM_QOS_CLASSES> max_wait_by_class_so_far_{};
    };
}