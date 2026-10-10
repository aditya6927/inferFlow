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
            throw std::runtime_error("Event time moved backwards");
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
    std::cout
        << "["
        << currentTime_
        << " ms] Request "
        << requestId
        << " arrived\n";

    admitRequest(requestId);
}

void Simulator::admitRequest(uint64_t requestId) {
    Request& request = getRequest(requestId);

    if (request.state != RequestState::Waiting) {
        return;
    }

    request.state = RequestState::Prefilling;
    request.admissionTime = currentTime_;

    activeRequests_.push_back(requestId);

    double prefillDuration =
        request.promptTokens * PREFILL_TIME_PER_TOKEN;

    events_.push({
        currentTime_ + prefillDuration,
        requestId,
        Event::Type::PrefillComplete
    });

    std::cout
        << "["
        << currentTime_
        << " ms] Request "
        << requestId
        << " started prefill\n";
}

void Simulator::handlePrefillComplete(uint64_t requestId) {
    Request& request = getRequest(requestId);

    request.state = RequestState::Decoding;

    std::cout
        << "["
        << currentTime_
        << " ms] Request "
        << requestId
        << " finished prefill\n";

    /*
     * If this is the first request ready to decode,
     * start the decode iterations.
     */
    if (request.generatedTokens == 0) {
        events_.push({
            currentTime_,
            request.id,
            Event::Type::DecodeIteration
        });
    }
}

void Simulator::handleDecodeIteration() {
    bool hasDecodingRequest = false;

    /*
     * One iteration represents one forward pass over
     * the currently active decoding batch.
     *
     * This is intentionally a simple synthetic model:
     *
     * iteration time =
     *     base cost + cost per active request
     *
     * It is NOT a calibrated hardware model.
     */
    const double iterationDuration =
        BASE_DECODE_TIME +
        DECODE_TIME_PER_REQUEST * activeRequests_.size();

    for (uint64_t requestId : activeRequests_) {
        Request& request = getRequest(requestId);

        if (request.state != RequestState::Decoding) {
            continue;
        }

        hasDecodingRequest = true;

        if (request.generatedTokens == 0) {
            request.firstTokenTime = currentTime_;

            std::cout
                << "["
                << currentTime_
                << " ms] Request "
                << request.id
                << " generated first token\n";
        }

        request.generatedTokens++;

        if (request.isComplete()) {
            request.state = RequestState::Completed;
            request.completionTime = currentTime_;

            std::cout
                << "["
                << currentTime_
                << " ms] Request "
                << request.id
                << " completed\n";
        }
    }

    if (!hasDecodingRequest) {
        return;
    }

    /*
     * Remove completed requests from the active batch.
     */
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

    /*
     * Schedule the next iteration if requests remain.
     */
    if (!activeRequests_.empty()) {
        events_.push({
            currentTime_ + iterationDuration,
            0,
            Event::Type::DecodeIteration
        });
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