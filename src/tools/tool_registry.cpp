// src/tools/tool_registry.cpp — YOU IMPLEMENT (spec §4.3).
//
// A hand-written hash table. No std::unordered_map / std::map / std::set.
// Separate chaining with a maximum load factor of 1.0.

#include "tools/tool_registry.h"
#include <cstdint>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(16) {}
ToolRegistry::~ToolRegistry() = default;   // unique_ptrs in your buckets free the tools

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(h);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool || find(tool->name()) != nullptr) {
        return false;
    }

    // Grow before the next insertion would exceed a load factor of 1.0.
    if (size_ == buckets_.size()) {
        std::vector<std::vector<std::unique_ptr<Tool>>> new_buckets(buckets_.size() * 2);
        for (auto& bucket : buckets_) {
            for (auto& existing : bucket) {
                const auto index = hash(existing->name()) % new_buckets.size();
                new_buckets[index].push_back(std::move(existing));
            }
        }
        buckets_ = std::move(new_buckets);
    }

    const auto index = hash(tool->name()) % buckets_.size();
    buckets_[index].push_back(std::move(tool));
    ++size_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const auto& bucket = buckets_[hash(name) % buckets_.size()];
    for (const auto& tool : bucket) {
        if (tool->name() == name) {
            return tool.get();
        }
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return size_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& bucket : buckets_) {
        for (const auto& tool : bucket) {
            fn(*tool);
        }
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(size_) / buckets_.size();
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const auto& bucket : buckets_) {
        if (bucket.size() > longest) {
            longest = bucket.size();
        }
    }
    return longest;
}
