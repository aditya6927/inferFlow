#pragma once

#include "request.hpp"

#include <vector>

struct RequestMetrics {
    double queueLatency;
    double ttft;
    double e2eLatency;
};

struct AggregateMetrics {
    double averageQueueLatency;
    double averageTTFT;
    double averageE2ELatency;

    double p50TTFT;
    double p95TTFT;

    double throughput;
};

RequestMetrics calculateMetrics(const Request& request);

AggregateMetrics calculateAggregateMetrics(
    const std::vector<Request>& requests
);