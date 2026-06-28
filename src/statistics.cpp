#include "statistics.h"

#include <algorithm>

namespace netsim {
    double ClassStats::drop_rate_percent() const {
        if (total_packets == 0) {
            return 0.0;
        }

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

        const double wait_us =
            static_cast<double>(wait_time(packet).count());

        const double sojourn_us =
            static_cast<double>(sojourn_time(packet).count());

        cs.avg_wait_time_us +=
            (wait_us - cs.avg_wait_time_us)
            / static_cast<double>(cs.transmitted_packets);

        cs.avg_sojourn_time_us +=
            (sojourn_us - cs.avg_sojourn_time_us)
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
        if (total_packets_ == 0) {
            return 0.0;
        }

        return 100.0 * static_cast<double>(total_dropped_)
                     / static_cast<double>(total_packets_);
    }

    double StatisticsCollector::overall_avg_wait_time_us() const {
        const uint32_t transmitted =
            total_packets_ - total_dropped_;

        if (transmitted == 0) {
            return 0.0;
        }

        return sum_wait_time_us_ / static_cast<double>(transmitted);
    }

    double StatisticsCollector::overall_avg_sojourn_time_us() const {
        const uint32_t transmitted =
            total_packets_ - total_dropped_;

        if (transmitted == 0) {
            return 0.0;
        }

        return sum_sojourn_time_us_ / static_cast<double>(transmitted);
    }

    double StatisticsCollector::overall_max_wait_time_us() const {
        return max_wait_time_us_;
    }

    double StatisticsCollector::jain_fairness_index() const {
        double sum = 0.0;
        double sum_squares = 0.0;

        for (const ClassStats& cs : class_stats_) {
            const double x =
                static_cast<double>(cs.transmitted_packets);

            sum += x;
            sum_squares += x * x;
        }

        if (sum_squares == 0.0) {
            return 0.0;
        }

        return (sum * sum)
             / (static_cast<double>(NUM_QOS_CLASSES) * sum_squares);
    }

    double StatisticsCollector::objective_score() const {
        constexpr double global_score_weight = 0.70;
        constexpr double class_score_weight = 0.20;
        constexpr double priority_penalty_weight = 0.10;

        constexpr double wait_weight = 0.25;
        constexpr double loss_weight = 0.45;
        constexpr double max_wait_weight = 0.20;
        constexpr double unfairness_weight = 0.10;

        constexpr double class_wait_weight = 0.30;
        constexpr double class_loss_weight = 0.50;
        constexpr double class_max_wait_weight = 0.20;

        constexpr double reference_wait_us = 100'000.0;

        constexpr double voice_wait_limit_us = 20'000.0;
        constexpr double http_wait_limit_us = 80'000.0;
        constexpr double file_wait_limit_us = 300'000.0;

        const double normalized_wait =
            overall_avg_wait_time_us() / reference_wait_us;

        const double normalized_loss =
            overall_drop_rate_percent() / 100.0;

        const double normalized_max_wait =
            overall_max_wait_time_us() / reference_wait_us;

        const double unfairness =
            1.0 - jain_fairness_index();

        const double global_score =
            wait_weight * normalized_wait +
            loss_weight * normalized_loss +
            max_wait_weight * normalized_max_wait +
            unfairness_weight * unfairness;

        auto class_cost = [&](QoSClass qos_class, double wait_limit_us) {
            const ClassStats& stats =
                class_stats_[static_cast<std::size_t>(qos_class)];

            const double total_packets =
                static_cast<double>(stats.total_packets);

            const double dropped_packets =
                static_cast<double>(stats.dropped_packets);

            const double avg_wait_us =
                stats.avg_wait_time_us;

            const double max_wait_us =
                stats.max_wait_time_us;

            const double drop_rate =
                total_packets > 0.0
                    ? dropped_packets / total_packets
                    : 0.0;

            const double normalized_class_wait =
                std::min(1.0, avg_wait_us / wait_limit_us);

            const double normalized_class_max_wait =
                std::min(1.0, max_wait_us / wait_limit_us);

            return class_wait_weight * normalized_class_wait +
                   class_loss_weight * drop_rate +
                   class_max_wait_weight * normalized_class_max_wait;
        };

        const double voice_cost =
            class_cost(QoSClass::Voice, voice_wait_limit_us);

        const double http_cost =
            class_cost(QoSClass::HTTP, http_wait_limit_us);

        const double file_cost =
            class_cost(QoSClass::File, file_wait_limit_us);

        const double class_priority_score =
            0.50 * voice_cost +
            0.30 * http_cost +
            0.20 * file_cost;

        const double priority_violation_penalty =
            (
                std::max(0.0, voice_cost - http_cost) +
                std::max(0.0, voice_cost - file_cost) +
                0.5 * std::max(0.0, http_cost - file_cost)
            ) / 2.5;

        return global_score_weight * global_score +
               class_score_weight * class_priority_score +
               priority_penalty_weight * priority_violation_penalty;
    }
}