#include "SenderSession.h"
#include "DxgiCapturer.h"
#include "Lz4Compressor.h"
#include "UdpSender.h"
#include "Log.h"

#include <mmsystem.h>
#include <chrono>
#include <stdexcept>
#include <vector>

namespace OxStream::SenderApp {
namespace {
using Clock = std::chrono::steady_clock;

struct TimerResolution {
    bool active = timeBeginPeriod(1) == TIMERR_NOERROR;
    ~TimerResolution() { if (active) timeEndPeriod(1); }
};

struct SessionError {
    std::wstring message;
    int field = -1;
};
} // namespace

SenderSession::~SenderSession() {
    RequestStop();
    Join();
}

void SenderSession::Start(Config config, HWND window) {
    if (m_thread.joinable()) throw std::logic_error("上次发送会话尚未结束");
    m_stop.store(false);
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status = {};
        m_status.done = false;
    }
    try {
        m_thread = std::thread([this, config, window] { Run(config, window); });
    } catch (...) {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.done = true;
        throw;
    }
}

void SenderSession::RequestStop() {
    // 与 wait_until 的条件检查使用同一互斥量，避免低 FPS 下丢失唤醒。
    {
        std::lock_guard<std::mutex> lock(m_waitMutex);
        m_stop.store(true);
    }
    m_wake.notify_all();
}

void SenderSession::Join() {
    if (m_thread.joinable()) m_thread.join();
}

SessionStatus SenderSession::Snapshot() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_status;
}

void SenderSession::Run(const Config& config, HWND window) {
    // CaptureAndSend 返回/抛出后，其所有 DXGI、WinSock 和定时器资源均已释放。
    try {
        CaptureAndSend(config, window);
    } catch (const SessionError& error) {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.error = error.message;
        m_status.invalidField = error.field;
    } catch (const std::exception& error) {
        try { SX_LOG_ERROR("[OxStream] 会话异常: {}", error.what()); } catch (...) {}
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.error = L"发送会话发生异常，已停止。详情请查看日志。";
    } catch (...) {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.error = L"发送会话发生未知异常，已停止。";
    }
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.done = true;
    }
    PostMessageW(window, kSessionChanged, 0, 0);
}

void SenderSession::CaptureAndSend(const Config& config, HWND window) {
    TimerResolution timer;
    // 保留 Host 对发送循环线程的调度设置，只作用于工作线程。
    if (!SetThreadAffinityMask(GetCurrentThread(), 1ULL << 2)) {
        SX_LOG_WARN("[OxStream] 工作线程绑定核心 2 失败: {}", GetLastError());
    }
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    SynapseX::DxgiCapturer capturer;
    if (!capturer.Initialize(config.width, config.height))
        throw SessionError{L"屏幕采集初始化失败。请查看日志中的 DXGI 错误。"};
    if (config.width > capturer.GetOutputWidth())
        throw SessionError{L"ROI 宽度超过当前采集屏幕宽度（" +
                           std::to_wstring(capturer.GetOutputWidth()) + L"）。", 2};
    if (config.height > capturer.GetOutputHeight())
        throw SessionError{L"ROI 高度超过当前采集屏幕高度（" +
                           std::to_wstring(capturer.GetOutputHeight()) + L"）。", 3};
    if (m_stop.load()) return;

    const int rawSize = config.width * config.height * 4;
    SynapseX::Lz4Compressor compressor;
    if (!compressor.Initialize(rawSize))
        throw SessionError{L"LZ4 压缩缓冲区初始化失败。"};
    SynapseX::UdpSender sender;
    if (!sender.Initialize(config.ip, static_cast<uint16_t>(config.port)))
        throw SessionError{L"UDP 发送器初始化失败。请查看日志中的网络错误。"};

    std::vector<uint8_t> rawBuffer, compressedBuffer, cachedCompressed;
    rawBuffer.reserve(rawSize);
    compressedBuffer.reserve(SynapseX::Lz4Compressor::GetMaxOutputSize(rawSize));
    cachedCompressed.reserve(SynapseX::Lz4Compressor::GetMaxOutputSize(rawSize));
    if (m_stop.load()) return;
    SX_LOG_INFO("[OxStream] 开始: {}:{} ROI={}x{} 目标帧率={}",
                config.ip, config.port, config.width, config.height, config.fps);
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.started = true;
    }
    PostMessageW(window, kSessionChanged, 0, 0);

    const auto interval = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / config.fps));
    auto nextTick = Clock::now();
    auto windowStart = nextTick;
    uint32_t frameId = 0;
    uint64_t capturedWindow = 0, sentWindow = 0, sentTotal = 0, failedTotal = 0;
    bool hasCachedFrame = false;

    while (!m_stop.load()) {
        const bool gotFrame = capturer.CaptureFrame(rawBuffer);
        // 显示模式改变后重新检查尺寸，避免把超出屏幕范围的 ROI 发给接收端。
        if (capturer.IsInitialized() &&
            (config.width > capturer.GetOutputWidth() || config.height > capturer.GetOutputHeight())) {
            throw SessionError{L"屏幕尺寸已改变，当前 ROI 超出屏幕范围，请调整后重新开始。",
                               config.width > capturer.GetOutputWidth() ? 2 : 3};
        }
        if (m_stop.load()) break;
        if (gotFrame) {
            ++capturedWindow;
            if (compressor.Compress(rawBuffer.data(), static_cast<int>(rawBuffer.size()), compressedBuffer)) {
                cachedCompressed = compressedBuffer;
                hasCachedFrame = true;
            }
        }
        if (hasCachedFrame) {
            const bool sent = sender.SendCompressedFrame(
                cachedCompressed.data(), static_cast<uint32_t>(cachedCompressed.size()), frameId,
                static_cast<uint16_t>(config.width), static_cast<uint16_t>(config.height), 0);
            if (sent) { ++sentTotal; ++sentWindow; }
            else ++failedTotal;
            ++frameId;
        }
        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - windowStart).count();
        {
            std::lock_guard<std::mutex> lock(m_statusMutex);
            m_status.failedFrames = failedTotal;
            m_status.sentFrames = sentTotal;
            if (elapsed >= 1.0) {
                m_status.captureFps = capturedWindow / elapsed;
                m_status.sendFps = sentWindow / elapsed;
                capturedWindow = sentWindow = 0;
                windowStart = now;
            }
        }
        nextTick += interval;
        const auto beforeWait = Clock::now();
        if (nextTick > beforeWait) {
            std::unique_lock<std::mutex> lock(m_waitMutex);
            m_wake.wait_until(lock, nextTick, [this] { return m_stop.load(); });
        } else {
            nextTick = beforeWait;
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_status.failedFrames = failedTotal;
        m_status.sentFrames = sentTotal;
        m_status.captureFps = m_status.sendFps = 0;
    }
    SX_LOG_INFO("[OxStream] 停止: 成功帧={} 失败帧={}", sentTotal, failedTotal);
}

} // namespace OxStream::SenderApp
