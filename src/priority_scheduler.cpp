#include "priority_scheduler.h"

#include <stdexcept>

namespace netsim {
    void PriorityScheduler::enqueue(uint32_t packet_id, QoSClass qos_class, uint32_t size_bytes) {
        (void)size_bytes;
        queues_[static_cast<std::size_t>(qos_class)].push(packet_id);
    }

    bool PriorityScheduler::has_packets() const {
        for (const auto& queue : queues_) {
            if (!queue.empty()) {
                return true;
            }
        }

        return false;
    }

    uint32_t PriorityScheduler::dequeue_next() {
        for (auto& queue : queues_) {
            if (!queue.empty()) {
                const uint32_t packet_id = queue.front();
                queue.pop();
                return packet_id;
            }
        }

        throw std::out_of_range("Priority scheduler queues are empty");
    }
}
