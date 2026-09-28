#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace OxStream::SenderApp {

struct Config {
    std::string ip = "192.168.100.2";
    int port = 8888;
    int width = 416;
    int height = 416;
    int fps = 170;
};

using ConfigFields = std::array<std::wstring, 5>;
ConfigFields ToFields(const Config& config);
// 逐字段验证，失败时返回应聚焦的输入框索引。
bool ParseConfig(const ConfigFields& fields, Config& config,
                 int& invalidField, std::wstring& error);
std::filesystem::path GetSettingsDirectory();
Config LoadConfig(const std::filesystem::path& directory);
bool SaveConfig(const std::filesystem::path& directory, const Config& config);

} // namespace OxStream::SenderApp
