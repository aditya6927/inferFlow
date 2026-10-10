#pragma once

#include "request.hpp"

#include <functional>
#include <queue>
#include <vector>

struct Event {
    double time;
    uint64_t requestId;

    enum class Type {
        Arrival,
        PrefillComplete,
        DecodeIteration
    };

    Type type;

    bool operator>(const Event& other) const {
        if (time != other.time)
            return time > other.time;

        return requestId > other.requestId;
    }
};

class Simulator {
public:
    const std::vector<Request>& requests() const;
    void addRequest(const Request& request);
    void run();

private:
    double currentTime_ = 0.0;

    std::vector<Request> requests_;

    std::priority_queue<
        Event,
        std::vector<Event>,
        std::greater<Event>
    > events_;

    std::vector<uint64_t> activeRequests_;

    void processEvent(const Event& event);

    void handleArrival(uint64_t requestId);
    void handlePrefillComplete(uint64_t requestId);
    void handleDecodeIteration();

    void admitRequest(uint64_t requestId);

    Request& getRequest(uint64_t requestId);

    bool isActive(uint64_t requestId) const;
};