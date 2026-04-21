#include "packet.h"
namespace netsim {
    const char* qos_name(QoSClass cls) {
        switch (cls) {
            case QoSClass::File: return "File";

            case QoSClass::Voice: return "Voice";

            case QoSClass::HTTP: return "HTTP";

            default: return "Unknown";
        }
    }

    SimTime wait_time(const Packet& p) {
        return p.dequeue_time - p.arrival_time;
    }

    SimTime sojourn_time(const Packet &p) {
        return p.departure_time - p.arrival_time;
    }

    std::ostream &operator<<(std::ostream &os, const Packet &p) {
        os << "[Packet#" << p.id << " " << qos_name(p.qos_class) << " " << p.size_bytes << "B]";
        return os;
    }
}
