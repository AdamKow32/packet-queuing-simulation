#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "packet.h"

namespace netsim {
    struct TrafficProfile {
        uint32_t packet_count{100};
        uint32_t seed{42};

        uint32_t min_interarrival_us{10};
        uint32_t max_interarrival_us{100};

        uint32_t voice_min_size_bytes{80};
        uint32_t voice_max_size_bytes{200};
        uint32_t http_min_size_bytes{500};
        uint32_t http_max_size_bytes{1500};
        uint32_t file_min_size_bytes{800};
        uint32_t file_max_size_bytes{1500};

        uint32_t voice_weight{20};
        uint32_t http_weight{50};
        uint32_t file_weight{30};
    };

    class TrafficGenerator {
    public:
        explicit TrafficGenerator(TrafficProfile profile);

        std::vector<Packet> generate() const;

    private:
        QoSClass sample_qos_class(std::mt19937& rng) const;
        uint32_t sample_packet_size(QoSClass qos_class, std::mt19937& rng) const;

        TrafficProfile profile_;
    };
}
