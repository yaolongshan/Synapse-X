#pragma once
#include "StageTiming.h"
#include <array>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace OxStream::SenderApp {
struct StageSnapshot {
    uint64_t window = 0;
    double seconds = 0, captureFps = 0, sendFps = 0;
    std::array<StageTiming, 4> stages{};
    uint64_t sent = 0, failed = 0;
};

// One in-flight write plus one replaceable snapshot. Disk I/O never holds mutex_.
class StageReporter {
public:
    using Write = std::function<void(const StageSnapshot&)>;
    explicit StageReporter(Write write) : write_(std::move(write)), worker_([this] { Run(); }) {}
    StageReporter(const StageReporter&) = delete;
    StageReporter& operator=(const StageReporter&) = delete;
    ~StageReporter() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
            pending_.reset();
        }
        wake_.notify_one();
        worker_.join(); // Only shutdown waits for an already active write.
    }
    bool Publish(const StageSnapshot& snapshot) {
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock() || stopping_) return false;
        pending_ = snapshot;
        lock.unlock();
        wake_.notify_one();
        return true;
    }
private:
    void Run() {
        for (;;) {
            StageSnapshot snapshot;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [this] { return stopping_ || pending_.has_value(); });
                if (stopping_) return;
                snapshot = *pending_;
                pending_.reset();
            }
            // A diagnostic sink failure must not terminate the sending session.
            try { write_(snapshot); } catch (...) {}
        }
    }
    Write write_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::optional<StageSnapshot> pending_;
    bool stopping_ = false;
    std::thread worker_;
};
} // namespace OxStream::SenderApp
