#include "SenderLog.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <fstream>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace SynapseX::SenderApp {
namespace {
// std::filesystem::path + ofstream 使用 Windows 原生宽字符路径，
// 避免修改 spdlog 全局文件名类型后与现有 Log.h 发生类型冲突。
class UnicodeFileSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit UnicodeFileSink(std::filesystem::path path) : m_path(std::move(path)) {
        std::filesystem::create_directories(m_path.parent_path());
        Open();
    }

protected:
    void sink_it_(const spdlog::details::log_msg& message) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(message, formatted);
        if (m_size + formatted.size() > kLimit && m_size > 0) Rotate();
        m_file.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
        if (!m_file) throw spdlog::spdlog_ex("写入 Sender 日志失败");
        m_size += formatted.size();
    }

    void flush_() override {
        m_file.flush();
        if (!m_file) throw spdlog::spdlog_ex("刷新 Sender 日志失败");
    }

private:
    std::filesystem::path Backup(int index) const {
        return m_path.parent_path() / (L"sender." + std::to_wstring(index) + L".log");
    }

    void Open() {
        m_file.open(m_path, std::ios::binary | std::ios::app);
        if (!m_file) throw spdlog::spdlog_ex("打开 Sender 日志失败");
        m_size = std::filesystem::file_size(m_path);
    }

    void Rotate() {
        m_file.close();
        try {
            std::filesystem::remove(Backup(3));
            for (int index = 2; index >= 1; --index) {
                if (std::filesystem::exists(Backup(index)))
                    std::filesystem::rename(Backup(index), Backup(index + 1));
            }
            std::filesystem::rename(m_path, Backup(1));
        } catch (...) {
            // 轮转失败后恢复当前文件，不能永久失去日志句柄。
            Open();
            throw;
        }
        Open();
    }

    static constexpr std::uintmax_t kLimit = 5 * 1024 * 1024;
    std::filesystem::path m_path;
    std::ofstream m_file;
    std::uintmax_t m_size = 0;
};
} // namespace

bool InitializeLogger(const std::filesystem::path& directory) {
    std::shared_ptr<spdlog::sinks::sink> sink;
    bool fileReady = true;
    try {
        sink = std::make_shared<UnicodeFileSink>(directory / L"logs" / L"sender.log");
    } catch (...) {
        fileReady = false;
        sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    }
    auto logger = std::make_shared<spdlog::logger>("sender", sink);
    logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [tid %t] %v");
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(logger);
    return fileReady;
}

} // namespace SynapseX::SenderApp
