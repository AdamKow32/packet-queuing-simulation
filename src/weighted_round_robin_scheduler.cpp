#include "weighted_round_robin_scheduler.h"

#include <stdexcept>

namespace netsim {
    WeightedRoundRobinScheduler::WeightedRoundRobinScheduler(std::array<uint32_t, NUM_QOS_CLASSES> weights)
        : weights_(weights) {
        build_schedule();
    }

    void WeightedRoundRobinScheduler::enqueue(uint32_t packet_id, QoSClass qos_class, uint32_t size_bytes) {
        (void)size_bytes;
        queues_[static_cast<std::size_t>(qos_class)].push(packet_id);
    }

    bool WeightedRoundRobinScheduler::has_packets() const {
        for (const auto& queue : queues_) {
            if (!queue.empty()) {
                return true;
            }
        }

        return false;
    }

    uint32_t WeightedRoundRobinScheduler::dequeue_next() {
        if (!has_packets()) {
            throw std::out_of_range("WRR scheduler queues are empty");
        }

        for (std::size_t checked = 0; checked < schedule_.size(); ++checked) {
            const std::size_t cls_index = schedule_[schedule_index_];
            schedule_index_ = (schedule_index_ + 1U) % schedule_.size();

            auto& queue = queues_[cls_index];
            if (!queue.empty()) {
                const uint32_t packet_id = queue.front();
                queue.pop();
                return packet_id;
            }
        }

        throw std::out_of_range("WRR scheduler queues are empty");
    }

    void WeightedRoundRobinScheduler::build_schedule() {
        for (const uint32_t weight : weights_) {
            if (weight == 0U) {
                throw std::invalid_argument("WRR weights must be positive");
            }
        }

        for (std::size_t cls_index = 0; cls_index < weights_.size(); ++cls_index) {
            for (uint32_t slot = 0; slot < weights_[cls_index]; ++slot) {
                schedule_.push_back(cls_index);
            }
        }
    }
}
