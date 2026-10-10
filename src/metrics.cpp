#include "metrics.hpp"

RequestMetrics calculateMetrics(const Request& request) {
    return {
        .queueLatency =
            request.admissionTime - request.arrivalTime,

        .ttft =
            request.firstTokenTime - request.arrivalTime,

        .e2eLatency =
            request.completionTime - request.arrivalTime
    };
}