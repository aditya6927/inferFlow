#include "kv_cache.hpp"

KVCache::KVCache(
    std::size_t totalBlocks,
    std::size_t blockSize
)
    : totalBlocks_(totalBlocks),
      blockSize_(blockSize) {}

std::size_t KVCache::blocksRequired(
    std::size_t tokens
) const {
    return (tokens + blockSize_ - 1) / blockSize_;
}

bool KVCache::canAllocate(std::size_t tokens) const {
    return blocksRequired(tokens) <= freeBlocks();
}

bool KVCache::allocate(
    uint64_t requestId,
    std::size_t tokens
) {
    if (allocations_.contains(requestId)) {
        return false;
    }

    const std::size_t blocks = blocksRequired(tokens);

    if (blocks > freeBlocks()) {
        return false;
    }

    allocations_[requestId] = blocks;
    usedBlocks_ += blocks;

    return true;
}

bool KVCache::grow(
    uint64_t requestId,
    std::size_t newTokenCount
) {
    auto it = allocations_.find(requestId);

    if (it == allocations_.end()) {
        return false;
    }

    const std::size_t newBlocks =
        blocksRequired(newTokenCount);

    const std::size_t oldBlocks = it->second;

    if (newBlocks <= oldBlocks) {
        return true;
    }

    const std::size_t additionalBlocks =
        newBlocks - oldBlocks;

    if (additionalBlocks > freeBlocks()) {
        return false;
    }

    it->second = newBlocks;
    usedBlocks_ += additionalBlocks;

    return true;
}

void KVCache::release(uint64_t requestId) {
    auto it = allocations_.find(requestId);

    if (it == allocations_.end()) {
        return;
    }

    usedBlocks_ -= it->second;
    allocations_.erase(it);
}

std::size_t KVCache::freeBlocks() const {
    return totalBlocks_ - usedBlocks_;
}

std::size_t KVCache::allocatedBlocks(
    uint64_t requestId
) const {
    auto it = allocations_.find(requestId);

    if (it == allocations_.end()) {
        return 0;
    }

    return it->second;
}