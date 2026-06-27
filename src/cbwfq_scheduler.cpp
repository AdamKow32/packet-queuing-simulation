#include "cbwfq_scheduler.h"

#include <algorithm>
#include <cmath>
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
        const std::size_t cls_index = static_cast<std::size_t>(qos_class);

        const double packet_bits =
            8.0 * static_cast<double>(std::max(size_bytes, 1U));

        const double weight =
            static_cast<double>(weights_[cls_index]);


        const double start_time =
            std::max(class_finish_times_[cls_index], virtual_time_);

        const double finish_time =
            start_time + packet_bits / weight;

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

            if (queue.empty()) {
                continue;
            }

            const double candidate_finish = queue.front().virtual_finish;

            if (candidate_finish < best_finish ||
                (std::abs(candidate_finish - best_finish) < 1e-9 && cls_index < best_class)) {
                best_class = cls_index;
                best_finish = candidate_finish;
            }
        }

        auto& queue = queues_[best_class];
        const QueuedPacket packet = queue.front();
        queue.pop();
        virtual_time_ = std::max(virtual_time_, packet.virtual_finish);

        return packet.packet_id;
    }
}
