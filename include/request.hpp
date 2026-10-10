#pragma once

#include <cstdint>

enum class RequestState {
    Waiting,
    Prefilling,
    Decoding,
    Completed
};

struct Request {
    uint64_t id;

    double arrivalTime;
    int promptTokens;
    int maxOutputTokens;

    int generatedTokens = 0;

    RequestState state = RequestState::Waiting;

    double admissionTime = -1.0;
    double firstTokenTime = -1.0;
    double completionTime = -1.0;

    bool isComplete() const {
        return generatedTokens >= maxOutputTokens;
    }
};