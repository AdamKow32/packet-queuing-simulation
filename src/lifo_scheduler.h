#pragma once

#include <cstdint>
#include <vector>

#include "scheduler.h"

namespace netsim {
    class LifoScheduler final : public IScheduler {
    public:
        using IScheduler::enqueue;
        void enqueue(uint32_t packet_id, QoSClass qos_class, uint32_t size_bytes) override;
        bool has_packets() const override;
        uint32_t dequeue_next() override;

    private:
        std::vector<uint32_t> stack_;
    };
}
