#pragma once

#include <cstdint>

#include "packet.h"

namespace netsim {
    enum class EventType : uint8_t {
        PacketArrival = 0,
        TransmissionComplete = 1
    };

    struct Event {
        SimTime time{};
        EventType type{EventType::PacketArrival};
        uint32_t packet_id{0};
    };

    struct EventCompare {
        bool operator()(const Event& lhs, const Event& rhs) const {
            if (lhs.time != rhs.time) {
                return lhs.time > rhs.time;
            }

            return static_cast<uint8_t>(lhs.type) > static_cast<uint8_t>(rhs.type);
        }
    };

    inline const char* event_name(EventType type) {
        switch (type) {
            case EventType::PacketArrival:
                return "PacketArrival";
            case EventType::TransmissionComplete:
                return "TransmissionComplete";
            default:
                return "Unknown";
        }
    }
}
