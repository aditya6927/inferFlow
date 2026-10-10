# inferFlow

A discrete-event simulator of LLM inference serving, written in C++20. It models how a serving engine admits requests under KV-cache memory pressure and schedules them with iteration-level (continuous) batching, and reports the latency metrics that matter for serving: queue latency, time-to-first-token (TTFT), end-to-end latency, and P50/P95 TTFT.

This is an early-stage project. It models the scheduling and memory-admission logic, not real model execution.

## What it models

- **Request lifecycle:** `Waiting -> Prefilling -> Decoding -> Completed`.
- **Event-driven core:** a priority queue of timed events (arrival, prefill complete, decode iteration, decode complete) with deterministic tie-breaking for simultaneous events.
- **Continuous batching:** a single global decode loop. Each iteration, every decoding request generates one token. Requests join the batch at iteration boundaries, and iteration time grows with batch size.
- **KV-cache admission control:** the cache is split into fixed-size blocks (currently 64 blocks x 16 tokens). A request is admitted only if its full KV footprint (prompt + max output tokens) fits. Admission is FIFO, so requests wait when the cache is full.
- **Memory release at the right time:** KV blocks are freed when a decode iteration *ends*, and only then are waiting requests admitted.

## Project layout

```
include/
  request.hpp     request state and timestamps
  kv_cache.hpp    block-based KV-cache accounting
  metrics.hpp     per-request and aggregate metrics
  simulator.hpp   event loop and scheduler
src/
  main.cpp        demo workload and report
  simulator.cpp   event handling, admission, decode loop
  kv_cache.cpp    allocate / grow / release
  metrics.cpp     TTFT, latency, percentiles, throughput
```

## Build and run

```bash
# CMake
cmake -S . -B build && cmake --build build
./build/inferFlow

# or directly with g++
g++ -std=c++20 -O2 -Wall -Wextra -Iinclude src/*.cpp -o inferFlow
./inferFlow
```

## Example output

Three requests share a KV cache of 1024 tokens (64 blocks x 16 tokens). Request 1 needs 600 tokens (38 blocks), but only 7 blocks are free while requests 2 and 3 run. It queues until request 2 finishes and frees its blocks.

```
Request   Queue(ms)      TTFT(ms)       E2E(ms)
-----------------------------------------------
1         116.40         122.40         221.40
2         0.00           5.00           123.40
3         0.00           4.20           122.40

Average Queue Latency: 38.80 ms
Average TTFT: 43.87 ms
P50 TTFT: 5.00 ms
P95 TTFT: 122.40 ms
Average E2E Latency: 155.73 ms
Throughput: 13 requests/s
```

Times are simulated time units, printed as milliseconds. The timing constants (prefill cost per token, decode base time and per-request cost) are illustrative and not calibrated against real hardware.

## Correctness notes

Issues found while testing, and fixed:

- **Causality:** a waiting request could be admitted before the request freeing its KV blocks had finished. Release and admission now happen at the end of the decode iteration.
- **Head-of-line blocking:** a request larger than the whole cache could never run and blocked the FIFO queue forever. Such requests are now rejected at submission.
- **Scaling:** request lookup was a linear scan. With an id-to-index map, 40,000 simulated requests run in about 0.13 s (previously about 49 s).
- **Percentiles:** now nearest-rank. The earlier floor-based index made P95 equal the median on small samples.
- **Metrics:** requests that never complete are excluded from aggregates instead of producing negative values.

## Limitations and roadmap

Not implemented yet:

- On-demand (paged) KV allocation with preemption when the cache is full. `KVCache::grow()` exists but the scheduler currently reserves the full footprint up front.
- Prefill/decode compute contention (prefill currently runs for free alongside decode).
- Calibrating the timing model against measurements from a real model.
- A configurable workload generator (Poisson arrivals, prompt/output length distributions) and CLI/config for cache size.
- Output-tokens/s throughput and KV-utilization metrics.
- Alternative scheduling policies compared against FIFO.
- Unit tests.