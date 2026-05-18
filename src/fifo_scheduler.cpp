#include "fifo_scheduler.h"

#include <stdexcept>

namespace netsim {

    FifoScheduler::FifoScheduler(size_t max_size)
    : max_size_(max_size) {}

    bool FifoScheduler::enqueue(uint32_t packet_id) {
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            return false;
        }
        queue_.push(packet_id);
        return true;
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
