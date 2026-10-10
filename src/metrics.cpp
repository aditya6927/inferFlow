#include "metrics.hpp"

#include <algorithm>
#include <numeric>

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

AggregateMetrics calculateAggregateMetrics(
    const std::vector<Request>& requests
) {
    if (requests.empty()) {
        return {};
    }

    std::vector<double> ttfts;
    ttfts.reserve(requests.size());

    double totalQueue = 0.0;
    double totalTTFT = 0.0;
    double totalE2E = 0.0;

    double startTime = requests.front().arrivalTime;
    double endTime = requests.front().completionTime;

    for (const Request& request : requests) {
        RequestMetrics metrics = calculateMetrics(request);

        totalQueue += metrics.queueLatency;
        totalTTFT += metrics.ttft;
        totalE2E += metrics.e2eLatency;

        ttfts.push_back(metrics.ttft);

        startTime = std::min(startTime, request.arrivalTime);
        endTime = std::max(endTime, request.completionTime);
    }

    std::sort(ttfts.begin(), ttfts.end());

    auto percentile = [&](double p) {
        std::size_t index =
            static_cast<std::size_t>(
                p * (ttfts.size() - 1)
            );

        return ttfts[index];
    };

    return {
        .averageQueueLatency =
            totalQueue / requests.size(),

        .averageTTFT =
            totalTTFT / requests.size(),

        .averageE2ELatency =
            totalE2E / requests.size(),

        .p50TTFT =
            percentile(0.50),

        .p95TTFT =
            percentile(0.95),

        .throughput =
            requests.size() / (endTime - startTime)
    };
}