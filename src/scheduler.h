#pragma once

#include <cstdint>

#include "packet.h"

namespace netsim {
    class IScheduler {
    public:
        virtual ~IScheduler() = default;

        virtual void enqueue(uint32_t packet_id, QoSClass qos_class) = 0;
        virtual bool has_packets() const = 0;
        virtual uint32_t dequeue_next() = 0;
    };
}
