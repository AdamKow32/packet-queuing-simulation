#pragma once

#include <array>
#include <cstdint>
#include <queue>

#include "scheduler.h"

namespace netsim {
    class CbwfqScheduler final : public IScheduler {
    public:
        explicit CbwfqScheduler(std::array<uint32_t, NUM_QOS_CLASSES> weights = {50U, 30U, 20U});

        using IScheduler::enqueue;
        void enqueue(uint32_t packet_id, QoSClass qos_class, uint32_t size_bytes) override;
        bool has_packets() const override;
        uint32_t dequeue_next() override;

    private:
        struct QueuedPacket {
            uint32_t packet_id;
            double virtual_finish;
        };

        std::array<std::queue<QueuedPacket>, NUM_QOS_CLASSES> queues_{};
        std::array<uint32_t, NUM_QOS_CLASSES> weights_{};
        std::array<double, NUM_QOS_CLASSES> class_finish_times_{};
        double virtual_time_{0.0};
    };
}
