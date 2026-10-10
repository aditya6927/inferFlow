#include "metrics.hpp"
#include "simulator.hpp"

#include <iomanip>
#include <iostream>

int main() {
    Simulator simulator;

    simulator.addRequest({
        .id = 1,
        .arrivalTime = 10.0,
        .promptTokens = 500,
        .maxOutputTokens = 100
    });

    simulator.addRequest({
        .id = 2,
        .arrivalTime = 3.0,
        .promptTokens = 400,
        .maxOutputTokens = 100
    });

    simulator.addRequest({
        .id = 3,
        .arrivalTime = 7.0,
        .promptTokens = 300,
        .maxOutputTokens = 100
    });

    simulator.run();

    std::cout << "\nRequest Metrics\n";
    std::cout << "===============\n";

    std::cout
        << std::left
        << std::setw(10) << "Request"
        << std::setw(15) << "Queue(ms)"
        << std::setw(15) << "TTFT(ms)"
        << std::setw(15) << "E2E(ms)"
        << '\n';

    std::cout
        << "-----------------------------------------------\n";

    for (const Request& request : simulator.requests()) {
        RequestMetrics metrics =
            calculateMetrics(request);

        std::cout
            << std::left
            << std::setw(10) << request.id
            << std::setw(15)
            << std::fixed
            << std::setprecision(2)
            << metrics.queueLatency
            << std::setw(15)
            << metrics.ttft
            << std::setw(15)
            << metrics.e2eLatency
            << '\n';
    }

    AggregateMetrics aggregate =
        calculateAggregateMetrics(simulator.requests());

    std::cout << "\nAggregate Metrics\n";
    std::cout << "=================\n";

    std::cout
        << "Average Queue Latency: "
        << aggregate.averageQueueLatency
        << " ms\n";

    std::cout
        << "Average TTFT: "
        << aggregate.averageTTFT
        << " ms\n";

    std::cout
        << "P50 TTFT: "
        << aggregate.p50TTFT
        << " ms\n";

    std::cout
        << "P95 TTFT: "
        << aggregate.p95TTFT
        << " ms\n";

    std::cout
        << "Average E2E Latency: "
        << aggregate.averageE2ELatency
        << " ms\n";

    std::cout
        << "Throughput: "
        << long(aggregate.throughput * 1000.0)
        << " requests/s\n";
}