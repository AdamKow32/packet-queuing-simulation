#pragma once

#include <array>
#include <cstdint>
#include <queue>
#include <vector>

#include "scheduler.h"

namespace netsim {
    class WeightedRoundRobinScheduler final : public IScheduler {
    public:
        explicit WeightedRoundRobinScheduler(std::array<uint32_t, NUM_QOS_CLASSES> weights = {4U, 2U, 1U});

        using IScheduler::enqueue;
        void enqueue(uint32_t packet_id, QoSClass qos_class, uint32_t size_bytes) override;
        bool has_packets() const override;
        uint32_t dequeue_next() override;

    private:
        void build_schedule();

        std::array<std::queue<uint32_t>, NUM_QOS_CLASSES> queues_{};
        std::array<uint32_t, NUM_QOS_CLASSES> weights_{};
        std::vector<std::size_t> schedule_;
        std::size_t schedule_index_{0};
    };
}
