#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

class KVCache {
public:
    KVCache(std::size_t totalBlocks, std::size_t blockSize);

    std::size_t blocksRequired(std::size_t tokens) const;

    bool canAllocate(std::size_t tokens) const;

    bool allocate(uint64_t requestId, std::size_t tokens);

    bool grow(uint64_t requestId, std::size_t newTokenCount);

    void release(uint64_t requestId);

    std::size_t freeBlocks() const;
    std::size_t allocatedBlocks(uint64_t requestId) const;

private:
    std::size_t totalBlocks_;
    std::size_t blockSize_;
    std::size_t usedBlocks_ = 0;

    std::unordered_map<uint64_t, std::size_t> allocations_;
};