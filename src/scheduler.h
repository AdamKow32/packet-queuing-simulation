#pragma once

#include <cstdint>

#include "packet.h"

namespace netsim {
    class IScheduler {
    public:
        virtual ~IScheduler() = default;

        virtual void enqueue(uint32_t packet_id, QoSClass qos_class, uint32_t size_bytes) = 0;
        void enqueue(uint32_t packet_id, QoSClass qos_class) {
            enqueue(packet_id, qos_class, 0U);
        }

        virtual bool has_packets() const = 0;
        virtual uint32_t dequeue_next() = 0;
    };
}
