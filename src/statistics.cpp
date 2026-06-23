/**
 * @file statistics.cpp
 * @brief Implementation of StatisticsCollector
 *
 * @date 15.04.2026
 */
#include "statistics.h"

namespace netsim {
    double ClassStats::drop_rate_percent() const {
        if (total_packets == 0) return 0.0;
        return 100.0 * static_cast<double>(dropped_packets)
                     / static_cast<double>(total_packets);
    }

    void StatisticsCollector::record(const Packet& packet) {
        const size_t index = static_cast<size_t>(packet.qos_class);
        ClassStats& cs = class_stats_[index];

        cs.total_packets++;
        total_packets_++;

        if (packet.dropped) {
            cs.dropped_packets++;
            total_dropped_++;
            return;
        }

        cs.transmitted_packets++;

        const double wait_us    = static_cast<double>(wait_time(packet).count());
        const double sojourn_us = static_cast<double>(sojourn_time(packet).count());

        cs.avg_wait_time_us += (wait_us - cs.avg_wait_time_us)
                                / static_cast<double>(cs.transmitted_packets);

        cs.avg_sojourn_time_us += (sojourn_us - cs.avg_sojourn_time_us)
                                   / static_cast<double>(cs.transmitted_packets);

        if (wait_us > cs.max_wait_time_us) {
            cs.max_wait_time_us = wait_us;
        }

        sum_wait_time_us_ += wait_us;
        sum_sojourn_time_us_ += sojourn_us;
        if (wait_us > max_wait_time_us_) {
            max_wait_time_us_ = wait_us;
        }
    }

    const ClassStats& StatisticsCollector::stats_for(QoSClass cls) const {
        return class_stats_[static_cast<size_t>(cls)];
    }

    double StatisticsCollector::overall_drop_rate_percent() const {
        if (total_packets_ == 0) return 0.0;
        return 100.0 * static_cast<double>(total_dropped_)
                     / static_cast<double>(total_packets_);
    }

    double StatisticsCollector::overall_avg_wait_time_us() const {
        const uint32_t transmitted = total_packets_ - total_dropped_;
        if (transmitted == 0) return 0.0;
        return sum_wait_time_us_ / static_cast<double>(transmitted);
    }

    double StatisticsCollector::overall_avg_sojourn_time_us() const {
        const uint32_t transmitted = total_packets_ - total_dropped_;
        if (transmitted == 0) return 0.0;
        return sum_sojourn_time_us_ / static_cast<double>(transmitted);
    }

    double StatisticsCollector::overall_max_wait_time_us() const {
        return max_wait_time_us_;
    }
}
