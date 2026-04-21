/**
 * @file packet.h
 * @brief Core data types for the packet simulator
 *
 * @date 15.04.2026
 */
#pragma once
#include <chrono>
#include <cstdint>
#include <ostream>
namespace netsim {
    // Simulation time unit - microseconds
    using SimTime = std::chrono::microseconds;

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

    SimTime wait_time(const Packet& p); // how long packet sat in queue doing nothing

    SimTime sojourn_time(const Packet& p); // total time from arrival until it finished transmitting

    std::ostream& operator<<(std::ostream& os, const Packet& p);
}
