/**
 * @file statistics.cpp
 * @brief Implementation of StatisticsCollector
 *
 * @date 15.04.2026
 */
#include "statistics.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

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

    void StatisticsCollector::print_summary() const {
        std::cout << "\n";
        std::cout << std::left
                  << std::setw(8)  << "Class"
                  << std::setw(8)  << "Total"
                  << std::setw(8)  << "Sent"
                  << std::setw(9)  << "Dropped"
                  << std::setw(9)  << "Drop%"
                  << std::setw(13) << "AvgWait(us)"
                  << std::setw(13) << "MaxWait(us)"
                  << "\n";

        for (const auto& cs : class_stats_) {
            std::cout << std::left
                      << std::setw(8)  << qos_name(cs.qos_class)
                      << std::setw(8)  << cs.total_packets
                      << std::setw(8)  << cs.transmitted_packets
                      << std::setw(9)  << cs.dropped_packets
                      << std::setw(9)  << std::fixed << std::setprecision(1)
                                       << cs.drop_rate_percent()
                      << std::setw(13) << std::fixed << std::setprecision(1)
                                       << cs.avg_wait_time_us
                      << std::setw(13) << std::fixed << std::setprecision(1)
                                       << cs.max_wait_time_us
                      << "\n";
        }

        std::cout << "Overall drop rate : "
                  << std::fixed << std::setprecision(2)
                  << overall_drop_rate_percent() << "%\n";
        std::cout << "Overall avg wait  : "
                  << std::fixed << std::setprecision(1)
                  << overall_avg_wait_time_us() << " us\n";
    }

    void StatisticsCollector::write_csv(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }

        file << "class,total,transmitted,dropped,drop_rate_pct,"
             << "avg_wait_us,max_wait_us,avg_sojourn_us\n";

        for (const auto& cs : class_stats_) {
            file << qos_name(cs.qos_class)    << ","
                 << cs.total_packets          << ","
                 << cs.transmitted_packets    << ","
                 << cs.dropped_packets        << ","
                 << std::fixed << std::setprecision(2)
                 << cs.drop_rate_percent()    << ","
                 << cs.avg_wait_time_us       << ","
                 << cs.max_wait_time_us       << ","
                 << cs.avg_sojourn_time_us    << "\n";
        }
    }

}