#pragma once

#include <cstdint>

namespace netsim {
    class IScheduler {
    public:
        virtual ~IScheduler() = default;

        virtual bool enqueue(uint32_t packet_id) = 0;
        virtual bool has_packets() const = 0;
        virtual uint32_t dequeue_next() = 0;
    };
}
