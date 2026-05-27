#include "cbwfq_scheduler.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace netsim {
    CbwfqScheduler::CbwfqScheduler(std::array<uint32_t, NUM_QOS_CLASSES> weights)
        : weights_(weights) {
        for (const uint32_t weight : weights_) {
            if (weight == 0U) {
                throw std::invalid_argument("CBWFQ weights must be positive");
            }
        }
    }

    void CbwfqScheduler::enqueue(uint32_t packet_id, QoSClass qos_class, uint32_t size_bytes) {
        if (!has_packets()) {
            virtual_time_ = 0.0;
            class_finish_times_.fill(0.0);
        }

        const std::size_t cls_index = static_cast<std::size_t>(qos_class);
        const double packet_size = static_cast<double>(std::max(size_bytes, 1U));
        const double weight = static_cast<double>(weights_[cls_index]);
        const double start_time = std::max(class_finish_times_[cls_index], virtual_time_);
        const double finish_time = start_time + packet_size / weight;

        class_finish_times_[cls_index] = finish_time;
        queues_[cls_index].push(QueuedPacket{packet_id, finish_time});
    }

    bool CbwfqScheduler::has_packets() const {
        for (const auto& queue : queues_) {
            if (!queue.empty()) {
                return true;
            }
        }

        return false;
    }

    uint32_t CbwfqScheduler::dequeue_next() {
        if (!has_packets()) {
            throw std::out_of_range("CBWFQ scheduler queues are empty");
        }

        std::size_t best_class = NUM_QOS_CLASSES;
        double best_finish = std::numeric_limits<double>::max();

        for (std::size_t cls_index = 0; cls_index < queues_.size(); ++cls_index) {
            const auto& queue = queues_[cls_index];
            if (!queue.empty() && queue.front().virtual_finish < best_finish) {
                best_class = cls_index;
                best_finish = queue.front().virtual_finish;
            }
        }

        auto& queue = queues_[best_class];
        const QueuedPacket packet = queue.front();
        queue.pop();
        virtual_time_ = std::max(virtual_time_, packet.virtual_finish);

        return packet.packet_id;
    }
}
