#pragma once

#include "kv_cache.hpp"
#include "request.hpp"

#include <cstdint>
#include <queue>
#include <unordered_map>
#include <vector>

class Simulator {
public:
    Simulator() = default;

    void addRequest(const Request& request);
    void run();

    const std::vector<Request>& requests() const;

private:
    struct Event {
        enum class Type {
            Arrival,
            PrefillComplete,
            DecodeComplete,
            DecodeIteration
        };

        double time;
        uint64_t requestId;
        Type type;

        // Ensures deterministic ordering for simultaneous events.
        bool operator<(const Event& other) const {
            if (time != other.time) {
                return time > other.time;
            }

            if (type != other.type) {
                return static_cast<int>(type) >
                       static_cast<int>(other.type);
            }

            return requestId > other.requestId;
        }
    };

    void processEvent(const Event& event);

    void handleArrival(uint64_t requestId);
    void handlePrefillComplete(uint64_t requestId);
    void handleDecodeIteration();
    void handleDecodeComplete();

    void tryAdmitWaitingRequests();
    void admitRequest(uint64_t requestId);

    Request& getRequest(uint64_t requestId);
    bool isActive(uint64_t requestId) const;

    std::priority_queue<Event> events_;

    std::vector<Request> requests_;
    std::unordered_map<uint64_t, std::size_t> requestIndex_;
    std::vector<uint64_t> activeRequests_;
    std::queue<uint64_t> waitingQueue_;

    KVCache kvCache_{64, 16};

    double currentTime_ = 0.0;

    // True while a decode iteration is scheduled or in flight.
    bool decodeScheduled_ = false;
};