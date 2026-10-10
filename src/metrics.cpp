#include "metrics.hpp"

#include <algorithm>
#include <cmath>

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
    std::vector<double> ttfts;
    ttfts.reserve(requests.size());

    double totalQueue = 0.0;
    double totalTTFT = 0.0;
    double totalE2E = 0.0;

    double startTime = 0.0;
    double endTime = 0.0;

    for (const Request& request : requests) {
        // Requests that never completed have no valid timestamps.
        if (request.state != RequestState::Completed) {
            continue;
        }

        RequestMetrics metrics = calculateMetrics(request);

        totalQueue += metrics.queueLatency;
        totalTTFT += metrics.ttft;
        totalE2E += metrics.e2eLatency;

        ttfts.push_back(metrics.ttft);

        if (ttfts.size() == 1) {
            startTime = request.arrivalTime;
            endTime = request.completionTime;
        } else {
            startTime = std::min(startTime, request.arrivalTime);
            endTime = std::max(endTime, request.completionTime);
        }
    }

    const std::size_t completed = ttfts.size();

    if (completed == 0) {
        return {};
    }

    std::sort(ttfts.begin(), ttfts.end());

    // Nearest-rank percentile.
    auto percentile = [&](double p) {
        std::size_t rank = static_cast<std::size_t>(
            std::ceil(p * static_cast<double>(completed))
        );

        rank = std::clamp<std::size_t>(rank, 1, completed);

        return ttfts[rank - 1];
    };

    const double span = endTime - startTime;

    return {
        .averageQueueLatency =
            totalQueue / completed,

        .averageTTFT =
            totalTTFT / completed,

        .averageE2ELatency =
            totalE2E / completed,

        .p50TTFT =
            percentile(0.50),

        .p95TTFT =
            percentile(0.95),

        .throughput =
            span > 0.0 ? completed / span : 0.0
    };
}