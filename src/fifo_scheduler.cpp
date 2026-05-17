#include "fifo_scheduler.h"

#include <stdexcept>

namespace netsim {
    void FifoScheduler::enqueue(uint32_t packet_id) {
        queue_.push(packet_id);
    }

    bool FifoScheduler::has_packets() const {
        return !queue_.empty();
    }

    uint32_t FifoScheduler::dequeue_next() {
        if (queue_.empty()) {
            throw std::out_of_range("Scheduler queue is empty");
        }

        const uint32_t packet_id = queue_.front();
        queue_.pop();
        return packet_id;
    }
}
