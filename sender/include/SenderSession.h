#pragma once

#include "SenderConfig.h"
#include <winsock2.h>
#include <windows.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace SynapseX::SenderApp {

constexpr UINT kSessionChanged = WM_APP + 1;

struct SessionStatus {
    bool started = false;
    bool done = true;
    double captureFps = 0;
    double sendFps = 0;
    uint64_t failedFrames = 0;
    uint64_t sentFrames = 0;
    int invalidField = -1;
    std::wstring error;
};

class SenderSession {
public:
    ~SenderSession();
    SenderSession() = default;
    SenderSession(const SenderSession&) = delete;
    SenderSession& operator=(const SenderSession&) = delete;

    // 仅由 UI 线程调用；上次会话必须先 Join。
    void Start(Config config, HWND window);
    void RequestStop();
    void Join();
    SessionStatus Snapshot() const;

private:
    void Run(const Config& config, HWND window);
    void CaptureAndSend(const Config& config, HWND window);
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    mutable std::mutex m_statusMutex;
    SessionStatus m_status;
    std::mutex m_waitMutex;
    std::condition_variable m_wake;
};

} // namespace SynapseX::SenderApp
