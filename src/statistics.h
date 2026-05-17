#pragma once
#include <array>
#include "packet.h"

namespace netsim {
    struct ClassStats {
        QoSClass qos_class;

        uint32_t total_packets{0};
        uint32_t dropped_packets{0};
        uint32_t transmitted_packets{0};

        double avg_wait_time_us{0.0};
        double max_wait_time_us{0.0};
        double avg_sojourn_time_us{0.0};

        double drop_rate_percent() const;
    };

    class StatisticsCollector {
        void record(const Packet& packet);


        const ClassStats& stats_for(QoSClass cls) const;


        double overall_drop_rate_percent() const;


        double overall_avg_wait_time_us() const;


        void print_summary() const;


        void write_csv(const std::string& filename) const;

        std::array<ClassStats, NUM_QOS_CLASSES> class_stats_ = {{
            ClassStats{QoSClass::Voice},
            ClassStats{QoSClass::HTTP},
            ClassStats{QoSClass::File}
        }};

        uint32_t total_packets_{0};
        uint32_t total_dropped_{0};
        double   sum_wait_time_us_{0.0};
    };
}