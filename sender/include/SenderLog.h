#pragma once
#include <filesystem>

namespace SynapseX::SenderApp {
// GUI 专属日志，不改变共享 Log.h 的窄字符文件名配置。
bool InitializeLogger(const std::filesystem::path& directory);
}
