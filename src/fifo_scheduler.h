#pragma once

#include <cstdint>
#include <queue>

#include "scheduler.h"

namespace netsim {
    class FifoScheduler final : public IScheduler {
    public:
        explicit FifoScheduler(size_t max_size = 0);
        bool enqueue(uint32_t packet_id) override;
        bool has_packets() const override;
        uint32_t dequeue_next() override;

    private:
        std::queue<uint32_t> queue_;
        size_t max_size_;
    };
}
