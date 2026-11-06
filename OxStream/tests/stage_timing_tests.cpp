#include "StageTiming.h"
#include "StageReporter.h"
#include <future>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

bool near(double actual, double expected) {
    return std::abs(actual - expected) < 1e-9;
}

void reporter_cases() {
    using namespace OxStream::SenderApp;
    using namespace std::chrono_literals;
    std::promise<void> entered, release, newest;
    auto enteredFuture = entered.get_future();
    auto releaseFuture = release.get_future().share();
    auto newestFuture = newest.get_future();
    StageReporter reporter([&](const StageSnapshot& snapshot) {
        if (snapshot.window == 1) {
            entered.set_value();
            releaseFuture.wait(); // Simulate a blocked file write, without sleeping.
            throw std::runtime_error("sink failure");
        }
        if (snapshot.window == 3) newest.set_value();
    });
    StageSnapshot first; first.window = 1;
    // Publish may deliberately skip a snapshot during the worker's short critical section.
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    bool published = false;
    while (!(published = reporter.Publish(first)) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    const bool writing = published && enteredFuture.wait_for(2s) == std::future_status::ready;
    StageSnapshot second; second.window = 2;
    StageSnapshot third; third.window = 3;
    const bool acceptedSecond = writing && reporter.Publish(second);
    const bool acceptedThird = writing && reporter.Publish(third);
    release.set_value(); // Release before assertions so a failure still joins the worker.
    check(writing && acceptedSecond && acceptedThird, "publishing waited for or rejected active sink work");
    check(newestFuture.wait_for(2s) == std::future_status::ready,
          "latest snapshot or recovery after sink exception was lost");
    // Destructor also covers waking and joining an idle reporter.
}
}

int main() {
    try {
        reporter_cases();
        OxStream::SenderApp::StageTiming timing;
        check(timing.Count() == 0 && timing.MeanMs() == -1 && timing.MaxMs() == -1,
              "empty window must report missing timings");
        timing.Add(std::chrono::microseconds(1500));
        timing.Add(std::chrono::microseconds(4500));
        timing.Add(std::chrono::microseconds(3000));
        check(timing.Count() == 3, "sample count mismatch");
        check(near(timing.MeanMs(), 3.0), "mean or milliseconds conversion mismatch");
        check(near(timing.MaxMs(), 4.5), "maximum must retain the largest sample");
        timing.Reset();
        check(timing.Count() == 0 && timing.MeanMs() == -1 && timing.MaxMs() == -1,
              "reset must clear samples and restore missing timings");
        timing.Add(std::chrono::steady_clock::duration::zero());
        check(timing.Count() == 1 && timing.MeanMs() == 0 && timing.MaxMs() == 0,
              "zero-duration success must be distinct from a missing sample");
        timing.Add(std::chrono::microseconds(1000));
        check(timing.Count() == 2 && near(timing.MeanMs(), 0.5) && near(timing.MaxMs(), 1.0),
              "previous reporting window leaked after reset");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
