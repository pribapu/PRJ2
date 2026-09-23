// src/sentinel_scanner.cpp
#include "core/sentinel_scanner.h"

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(sentinel) {
}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    // Add the new chunk to any text left over from the previous call.
    pending_.append(chunk.data(), chunk.size());

    // Check if the sentinel appears in the pending text.
    std::size_t pos = pending_.find(sentinel_);

    if (pos != std::string::npos) {
        // Everything before the sentinel is safe to print.
        std::string safe = pending_.substr(0, pos);

        pending_.clear();

        return {safe, true};
    }

    // Keep enough characters in pending_ so that a sentinel split
    // between two chunks can still be detected.
    std::size_t keep = 0;

    if (!sentinel_.empty()) {
        keep = sentinel_.size() - 1;
    }

    // If there is not enough text to safely release anything yet,
    // keep waiting for another chunk.
    if (pending_.size() <= keep) {
        return {"", false};
    }

    // Everything except the possible sentinel prefix is safe to print.
    std::size_t safeLength = pending_.size() - keep;

    std::string safe = pending_.substr(0, safeLength);
    pending_ = pending_.substr(safeLength);

    return {safe, false};
}

SentinelScanner::Out SentinelScanner::flush() {
    // The stream is finished, so all remaining text is safe.
    std::string safe = pending_;

    pending_.clear();

    return {safe, false};
}