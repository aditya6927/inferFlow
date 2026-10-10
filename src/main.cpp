#include "metrics.hpp"
#include "simulator.hpp"

#include <iomanip>
#include <iostream>

int main() {
    Simulator simulator;

    simulator.addRequest({
        .id = 1,
        .arrivalTime = 10.0,
        .promptTokens = 512,
        .maxOutputTokens = 128
    });

    simulator.addRequest({
        .id = 2,
        .arrivalTime = 3.0,
        .promptTokens = 1024,
        .maxOutputTokens = 256
    });

    simulator.addRequest({
        .id = 3,
        .arrivalTime = 7.0,
        .promptTokens = 256,
        .maxOutputTokens = 64
    });

    simulator.run();

    std::cout << "\n";
    std::cout << "Request Metrics\n";
    std::cout << "===============\n";

    std::cout
        << std::left
        << std::setw(10) << "Request"
        << std::setw(15) << "Queue(ms)"
        << std::setw(15) << "TTFT(ms)"
        << std::setw(15) << "E2E(ms)"
        << '\n';

    std::cout << "-----------------------------------------------\n";

    for (const Request& request : simulator.requests()) {
        RequestMetrics metrics = calculateMetrics(request);

        std::cout
            << std::left
            << std::setw(10) << request.id
            << std::setw(15) << std::fixed << std::setprecision(2)
            << metrics.queueLatency
            << std::setw(15)
            << metrics.ttft
            << std::setw(15)
            << metrics.e2eLatency
            << '\n';
    }
}