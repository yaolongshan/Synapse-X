#include "Lz4Compressor.h"
#include "lz4.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using SynapseX::Lz4Compressor;

namespace {
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void checkDecoded(const std::vector<uint8_t>& compressed, int length,
                  const std::vector<uint8_t>& expected) {
    std::vector<uint8_t> decoded(expected.size());
    const int result = LZ4_decompress_safe(reinterpret_cast<const char*>(compressed.data()),
        reinterpret_cast<char*>(decoded.data()), length, static_cast<int>(decoded.size()));
    check(result == static_cast<int>(expected.size()) && decoded == expected, "round trip failed");
}
}

int main() {
    try {
        const int capacity = Lz4Compressor::GetMaxOutputSize(640 * 640 * 3);
        std::vector<uint8_t> work(capacity), cached(capacity);
        const auto first = work.data(), second = cached.data();
        const auto workCapacity = work.capacity(), cachedCapacity = cached.capacity();
        int cachedLength = 0;
        for (int width : {64, 65, 640, 64}) {
            std::vector<uint8_t> raw(width * 67 * 3);
            for (size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<uint8_t>(i * 37 + width);
            const int length = Lz4Compressor::CompressInto(raw.data(), static_cast<int>(raw.size()),
                work.data(), static_cast<int>(work.size()));
            check(length > 0, "direct compression failed");
            work.swap(cached);
            cachedLength = length;
            checkDecoded(cached, cachedLength, raw);
            // No new frame: the same cache and exact length remain reusable.
            checkDecoded(cached, cachedLength, raw);
            const auto* cachedPointer = cached.data();
            const int failed = Lz4Compressor::CompressInto(raw.data(), static_cast<int>(raw.size()), work.data(), 1);
            check(failed == 0, "insufficient destination was accepted");
            check(cached.data() == cachedPointer, "failure replaced cache");
            checkDecoded(cached, cachedLength, raw);
            check(work.size() == static_cast<size_t>(capacity) && cached.size() == static_cast<size_t>(capacity),
                  "working buffers were resized");
            check((work.data() == first && cached.data() == second && work.capacity() == workCapacity && cached.capacity() == cachedCapacity) ||
                  (work.data() == second && cached.data() == first && work.capacity() == cachedCapacity && cached.capacity() == workCapacity),
                  "working buffers were reallocated");
        }
        uint8_t input = 1;
        check(Lz4Compressor::CompressInto(nullptr, 1, work.data(), capacity) == 0, "null input accepted");
        check(Lz4Compressor::CompressInto(&input, 0, work.data(), capacity) == 0, "empty input accepted");
        check(Lz4Compressor::CompressInto(&input, -1, work.data(), capacity) == 0, "negative size accepted");
        check(Lz4Compressor::CompressInto(&input, LZ4_MAX_INPUT_SIZE + 1, work.data(), capacity) == 0, "oversized input accepted");
        check(Lz4Compressor::CompressInto(&input, 1, nullptr, capacity) == 0, "null output accepted");
        check(Lz4Compressor::CompressInto(&input, 1, work.data(), 0) == 0, "zero capacity accepted");
        check(Lz4Compressor::CompressInto(&input, 1, work.data(), -1) == 0, "negative capacity accepted");
        // Host still uses the original API, including growing beyond initialization size.
        Lz4Compressor legacy;
        check(legacy.Initialize(16), "legacy initialization failed");
        std::vector<uint8_t> raw(64 * 64 * 4, 91), output;
        check(legacy.Compress(raw.data(), static_cast<int>(raw.size()), output), "legacy compression failed");
        checkDecoded(output, static_cast<int>(output.size()), raw);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
