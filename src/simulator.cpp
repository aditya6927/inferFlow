#include "simulator.hpp"

#include <algorithm>
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

    if (requestIndex_.contains(request.id)) {
        throw std::invalid_argument("Duplicate request ID");
    }

    // A request that can never fit in the KV cache would block the
    // FIFO queue forever, so reject it up front.
    const std::size_t totalTokens =
        static_cast<std::size_t>(request.promptTokens) +
        static_cast<std::size_t>(request.maxOutputTokens);

    if (totalTokens > kvCache_.capacityTokens()) {
        throw std::invalid_argument(
            "Request exceeds total KV cache capacity"
        );
    }

    requestIndex_[request.id] = requests_.size();
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

        case Event::Type::DecodeComplete:
            handleDecodeComplete();
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

// Start of a decode iteration: every decoding request produces one token.
// The iteration takes time, so the tokens become available at its end, and
// KV blocks are only freed (and waiting requests admitted) at that moment,
// in handleDecodeComplete().
void Simulator::handleDecodeIteration() {
    // Only requests that are actually decoding take part in this step.
    std::size_t decodingCount = 0;

    for (uint64_t requestId : activeRequests_) {
        if (getRequest(requestId).state == RequestState::Decoding) {
            ++decodingCount;
        }
    }

    if (decodingCount == 0) {
        decodeScheduled_ = false;
        return;
    }

    const double iterationDuration =
        BASE_DECODE_TIME +
        DECODE_TIME_PER_REQUEST * decodingCount;

    const double iterationEnd = currentTime_ + iterationDuration;

    for (uint64_t requestId : activeRequests_) {
        Request& request = getRequest(requestId);

        if (request.state != RequestState::Decoding) {
            continue;
        }

        request.generatedTokens++;

        if (request.generatedTokens == 1) {
            request.firstTokenTime = iterationEnd;
        }

        if (request.isComplete()) {
            request.state = RequestState::Completed;
            request.completionTime = iterationEnd;
        }
    }

    // decodeScheduled_ stays true: one iteration in flight at a time.
    events_.push({
        iterationEnd,
        0,
        Event::Type::DecodeComplete
    });
}

// End of a decode iteration: free finished requests, admit waiting ones,
// and start the next iteration if anything is still decoding.
void Simulator::handleDecodeComplete() {
    for (uint64_t requestId : activeRequests_) {
        if (getRequest(requestId).state == RequestState::Completed) {
            kvCache_.release(requestId);
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

    tryAdmitWaitingRequests();

    decodeScheduled_ = false;

    const bool anyDecoding = std::any_of(
        activeRequests_.begin(),
        activeRequests_.end(),
        [this](uint64_t requestId) {
            return getRequest(requestId).state ==
                   RequestState::Decoding;
        }
    );

    if (anyDecoding) {
        events_.push({
            currentTime_,
            0,
            Event::Type::DecodeIteration
        });

        decodeScheduled_ = true;
    }
}

Request& Simulator::getRequest(uint64_t requestId) {
    auto it = requestIndex_.find(requestId);

    if (it == requestIndex_.end()) {
        throw std::runtime_error("Unknown request ID");
    }

    return requests_[it->second];
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