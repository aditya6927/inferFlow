#include "simulator.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {

constexpr double PREFILL_TIME_PER_TOKEN = 0.01;
constexpr double BASE_DECODE_TIME = 0.8;
constexpr double DECODE_TIME_PER_REQUEST = 0.2;

}

void Simulator::addRequest(const Request& request) {
    if (request.maxOutputTokens <= 0) {
        throw std::invalid_argument(
            "Request must have positive maxOutputTokens"
        );
    }

    if (request.promptTokens <= 0) {
        throw std::invalid_argument(
            "Request must have positive promptTokens"
        );
    }

    requests_.push_back(request);

    events_.push({
        request.arrivalTime,
        request.id,
        Event::Type::Arrival
    });
}

void Simulator::run() {
    while (!events_.empty()) {
        Event event = events_.top();
        events_.pop();

        if (event.time < currentTime_) {
            throw std::runtime_error(
                "Event time moved backwards"
            );
        }

        currentTime_ = event.time;

        processEvent(event);
    }
}

void Simulator::processEvent(const Event& event) {
    switch (event.type) {
        case Event::Type::Arrival:
            handleArrival(event.requestId);
            break;

        case Event::Type::PrefillComplete:
            handlePrefillComplete(event.requestId);
            break;

        case Event::Type::DecodeIteration:
            handleDecodeIteration();
            break;
    }
}

void Simulator::handleArrival(uint64_t requestId) {
    waitingQueue_.push(requestId);
    tryAdmitWaitingRequests();
}

void Simulator::tryAdmitWaitingRequests() {
    while (!waitingQueue_.empty()) {
        const uint64_t requestId = waitingQueue_.front();
        Request& request = getRequest(requestId);

        // Reserve the entire maximum KV footprint.
        const std::size_t requiredTokens =
            static_cast<std::size_t>(request.promptTokens) +
            static_cast<std::size_t>(request.maxOutputTokens);

        if (!kvCache_.canAllocate(requiredTokens)) {
            break;
        }

        waitingQueue_.pop();
        admitRequest(requestId);
    }
}

void Simulator::admitRequest(uint64_t requestId) {
    Request& request = getRequest(requestId);

    if (request.state != RequestState::Waiting) {
        return;
    }

    const std::size_t requiredTokens =
        static_cast<std::size_t>(request.promptTokens) +
        static_cast<std::size_t>(request.maxOutputTokens);

    if (!kvCache_.allocate(request.id, requiredTokens)) {
        throw std::runtime_error(
            "KV allocation failed after capacity check"
        );
    }

    request.state = RequestState::Prefilling;
    request.admissionTime = currentTime_;

    activeRequests_.push_back(requestId);

    const double prefillDuration =
        request.promptTokens * PREFILL_TIME_PER_TOKEN;

    events_.push({
        currentTime_ + prefillDuration,
        requestId,
        Event::Type::PrefillComplete
    });
}

void Simulator::handlePrefillComplete(uint64_t requestId) {
    Request& request = getRequest(requestId);

    if (request.state != RequestState::Prefilling) {
        return;
    }

    request.state = RequestState::Decoding;

    // There is exactly one global decode loop.
    if (!decodeScheduled_) {
        events_.push({
            currentTime_,
            0,
            Event::Type::DecodeIteration
        });

        decodeScheduled_ = true;
    }
}

void Simulator::handleDecodeIteration() {
    decodeScheduled_ = false;

    const std::size_t activeCount =
        activeRequests_.size();

    if (activeCount == 0) {
        tryAdmitWaitingRequests();
        return;
    }

    const double iterationDuration =
        BASE_DECODE_TIME +
        DECODE_TIME_PER_REQUEST * activeCount;

    bool hasDecodingRequest = false;

    for (uint64_t requestId : activeRequests_) {
        Request& request = getRequest(requestId);

        if (request.state != RequestState::Decoding) {
            continue;
        }

        hasDecodingRequest = true;

        request.generatedTokens++;

        // The token becomes available at the end of this decode step.
        const double tokenTime =
            currentTime_ + iterationDuration;

        if (request.generatedTokens == 1) {
            request.firstTokenTime = tokenTime;
        }

        if (request.isComplete()) {
            request.state = RequestState::Completed;
            request.completionTime = tokenTime;

            kvCache_.release(request.id);
        }
    }

    activeRequests_.erase(
        std::remove_if(
            activeRequests_.begin(),
            activeRequests_.end(),
            [this](uint64_t requestId) {
                return getRequest(requestId).state ==
                       RequestState::Completed;
            }
        ),
        activeRequests_.end()
    );

    // Newly freed KV space can admit waiting requests.
    tryAdmitWaitingRequests();

    if (hasDecodingRequest && !activeRequests_.empty()) {
        events_.push({
            currentTime_ + iterationDuration,
            0,
            Event::Type::DecodeIteration
        });

        decodeScheduled_ = true;
    }
}

Request& Simulator::getRequest(uint64_t requestId) {
    for (Request& request : requests_) {
        if (request.id == requestId) {
            return request;
        }
    }

    throw std::runtime_error("Unknown request ID");
}

bool Simulator::isActive(uint64_t requestId) const {
    return std::find(
        activeRequests_.begin(),
        activeRequests_.end(),
        requestId
    ) != activeRequests_.end();
}

const std::vector<Request>& Simulator::requests() const {
    return requests_;
}