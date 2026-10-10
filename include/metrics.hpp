#pragma once

#include "request.hpp"

struct RequestMetrics {
    double queueLatency;
    double ttft;
    double e2eLatency;
};

RequestMetrics calculateMetrics(const Request& request);