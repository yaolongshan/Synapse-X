#pragma once

#include <chrono>
#include <cstdint>

namespace OxStream::SenderApp {

// One reporting window; callers add only successful local stage durations.
class StageTiming {
public:
    void Add(std::chrono::steady_clock::duration duration) {
        const double milliseconds = std::chrono::duration<double, std::milli>(duration).count();
        ++m_count;
        m_totalMs += milliseconds;
        if (milliseconds > m_maxMs) m_maxMs = milliseconds;
    }

    uint64_t Count() const { return m_count; }
    // -1 denotes a missing sample, not a zero-duration operation.
    double MeanMs() const { return m_count ? m_totalMs / m_count : -1.0; }
    double MaxMs() const { return m_count ? m_maxMs : -1.0; }
    void Reset() { *this = {}; }

private:
    uint64_t m_count = 0;
    double m_totalMs = 0;
    double m_maxMs = 0;
};

} // namespace OxStream::SenderApp
