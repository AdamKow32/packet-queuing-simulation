#include "traffic_generator.h"

#include <random>
#include <stdexcept>

namespace netsim {
    TrafficGenerator::TrafficGenerator(TrafficProfile profile)
        : profile_(profile) {
        if (profile_.min_interarrival_us > profile_.max_interarrival_us) {
            throw std::invalid_argument("min_interarrival_us cannot exceed max_interarrival_us");
        }

        if (profile_.voice_min_size_bytes > profile_.voice_max_size_bytes) {
            throw std::invalid_argument("voice_min_size_bytes cannot exceed voice_max_size_bytes");
        }

        if (profile_.http_min_size_bytes > profile_.http_max_size_bytes) {
            throw std::invalid_argument("http_min_size_bytes cannot exceed http_max_size_bytes");
        }

        if (profile_.file_min_size_bytes > profile_.file_max_size_bytes) {
            throw std::invalid_argument("file_min_size_bytes cannot exceed file_max_size_bytes");
        }

        const uint32_t total_weight =
            profile_.voice_weight + profile_.http_weight + profile_.file_weight;

        if (total_weight == 0U) {
            throw std::invalid_argument("At least one QoS class weight must be positive");
        }
    }

    std::vector<Packet> TrafficGenerator::generate() const {
        std::mt19937 rng(profile_.seed);
        std::uniform_int_distribution<uint32_t> interarrival_dist(
            profile_.min_interarrival_us,
            profile_.max_interarrival_us);

        std::vector<Packet> packets;
        packets.reserve(profile_.packet_count);

        SimTime current_arrival{0};

        for (uint32_t i = 0; i < profile_.packet_count; ++i) {
            if (i > 0) {
                current_arrival += SimTime{interarrival_dist(rng)};
            }

            const QoSClass qos_class = sample_qos_class(rng);

            packets.push_back(Packet{
                i + 1,
                qos_class,
                sample_packet_size(qos_class, rng),
                current_arrival
            });
        }

        return packets;
    }

    QoSClass TrafficGenerator::sample_qos_class(std::mt19937& rng) const {
        std::discrete_distribution<int> class_dist{
            static_cast<double>(profile_.voice_weight),
            static_cast<double>(profile_.http_weight),
            static_cast<double>(profile_.file_weight)
        };

        switch (class_dist(rng)) {
            case 0:
                return QoSClass::Voice;
            case 1:
                return QoSClass::HTTP;
            case 2:
                return QoSClass::File;
            default:
                throw std::logic_error("Unexpected QoS class sample");
        }
    }

    uint32_t TrafficGenerator::sample_packet_size(QoSClass qos_class, std::mt19937& rng) const {
        switch (qos_class) {
            case QoSClass::Voice: {
                std::uniform_int_distribution<uint32_t> dist(
                    profile_.voice_min_size_bytes,
                    profile_.voice_max_size_bytes);
                return dist(rng);
            }
            case QoSClass::HTTP: {
                std::uniform_int_distribution<uint32_t> dist(
                    profile_.http_min_size_bytes,
                    profile_.http_max_size_bytes);
                return dist(rng);
            }
            case QoSClass::File: {
                std::uniform_int_distribution<uint32_t> dist(
                    profile_.file_min_size_bytes,
                    profile_.file_max_size_bytes);
                return dist(rng);
            }
            default:
                throw std::logic_error("Unexpected QoS class");
        }
    }
}
