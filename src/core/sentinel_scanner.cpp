#include "core/sentinel_scanner.h"

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(sentinel) {
}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    std::string combined = pending_ + std::string(chunk);

    std::size_t pos = combined.find(sentinel_);

    if (pos != std::string::npos) {
        std::string safe = combined.substr(0, pos);
        pending_.clear();

        return {safe, true};
    }

    if (sentinel_.empty()) {
        return {combined, true};
    }

    std::size_t hold = sentinel_.size() - 1;

    if (combined.size() <= hold) {
        pending_ = combined;
        return {"", false};
    }

    std::size_t safe_length = combined.size() - hold;

    std::string safe = combined.substr(0, safe_length);
    pending_ = combined.substr(safe_length);

    return {safe, false};
}

SentinelScanner::Out SentinelScanner::flush() {
    std::string safe = pending_;
    pending_.clear();

    return {safe, false};
}