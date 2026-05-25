#pragma once

#include <array>
#include <cstdint>
#include <queue>

#include "scheduler.h"

namespace netsim {
    class PriorityScheduler final : public IScheduler {
    public:
        void enqueue(uint32_t packet_id, QoSClass qos_class) override;
        bool has_packets() const override;
        uint32_t dequeue_next() override;

    private:
        std::array<std::queue<uint32_t>, NUM_QOS_CLASSES> queues_{};
    };
}
