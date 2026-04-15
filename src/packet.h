/*
 * @file packet.h
 * @brief Core data types for the packet simulator
 *
 * @date 15.04.2026
 */
#ifndef PACKET_H
#define PACKET_H
#include <chrono>
#include <cstdint>
#include <ostream>
namespace netsim {
    // Simulation time unit - microseconds
    using simTime = std::chrono::microseconds;

    enum class QoSClass : uint8_t {
        Voice = 0,  // Real time audio - low latency, small fixed-sized frames
        HTTP = 1,   // HTTP - medium priority, tolerates brief delays
        File = 2    // TCP - throughput matters, latency does not
    };

    // Number of QoS classes - hard coded
    static constexpr int NUM_QOS_CLASSES = 3;

    struct Packet {
        uint32_t id;            // Unique identifier increasing
        QoSClass qos_class;     // Traffic class, determines the scheduling priority
        uint32_t size_bytes;    // Payload size in bytes
        SimTime arrival_time;   // when this packet arrives at router

        bool dropped;           // was this packet dropped by scheduler?
        SimTime dequeue_time;   // when does packet leave the queue?
        SimTime departure_time; // when does packet finish transmitting?
    };

    const char* qos_name(QoSClass cls);

    SimTime wait_time(const Packet& p);

    SimTime sojourn_time(const Packet& p);

    std::ostream& operator<<(std::ostream& os, const Packet& p);
}
#endif //PACKET_H
