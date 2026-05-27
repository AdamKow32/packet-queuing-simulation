#include "lifo_scheduler.h"

#include <stdexcept>

namespace netsim {
    void LifoScheduler::enqueue(uint32_t packet_id, QoSClass qos_class, uint32_t size_bytes) {
        (void)qos_class;
        (void)size_bytes;
        stack_.push_back(packet_id);
    }

    bool LifoScheduler::has_packets() const {
        return !stack_.empty();
    }

    uint32_t LifoScheduler::dequeue_next() {
        if (stack_.empty()) {
            throw std::out_of_range("Scheduler stack is empty");
        }

        const uint32_t packet_id = stack_.back();
        stack_.pop_back();
        return packet_id;
    }
}
