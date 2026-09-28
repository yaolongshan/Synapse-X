#include "SenderConfig.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shlobj.h>
#include <iterator>
#include <stdexcept>

namespace OxStream::SenderApp {
namespace {
constexpr const wchar_t* kKeys[] = {L"ip", L"port", L"width", L"height", L"fps"};

std::wstring Trim(const std::wstring& text) {
    const auto first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    return text.substr(first, text.find_last_not_of(L" \t\r\n") - first + 1);
}

bool ParseNumber(const std::wstring& text, int low, int high, int& result) {
    if (text.empty()) return false;
    int value = 0;
    for (wchar_t ch : text) {
        if (ch < L'0' || ch > L'9') return false;
        const int digit = ch - L'0';
        if (value > (high - digit) / 10) return false;
        value = value * 10 + digit;
    }
    if (value < low || value > high) return false;
    result = value;
    return true;
}
} // namespace

ConfigFields ToFields(const Config& config) {
    return {std::wstring(config.ip.begin(), config.ip.end()), std::to_wstring(config.port),
            std::to_wstring(config.width), std::to_wstring(config.height), std::to_wstring(config.fps)};
}

bool ParseConfig(const ConfigFields& fields, Config& config,
                 int& invalidField, std::wstring& error) {
    Config parsed;
    const auto ip = Trim(fields[0]);
    IN_ADDR address{};
    if (InetPtonW(AF_INET, ip.c_str(), &address) != 1 ||
        (ntohl(address.s_addr) >> 24) == 0 || (ntohl(address.s_addr) >> 24) >= 224) {
        invalidField = 0;
        error = L"请输入有效的单播 IPv4 地址。";
        return false;
    }
    parsed.ip.assign(ip.begin(), ip.end());
    int* values[] = {&parsed.port, &parsed.width, &parsed.height, &parsed.fps};
    constexpr int lows[] = {1, 64, 64, 1};
    constexpr int highs[] = {65535, 4096, 4096, 1000};
    constexpr const wchar_t* names[] = {L"端口", L"ROI 宽度", L"ROI 高度", L"发送帧率"};
    for (int i = 0; i < 4; ++i) {
        if (!ParseNumber(Trim(fields[i + 1]), lows[i], highs[i], *values[i])) {
            invalidField = i + 1;
            error = std::wstring(names[i]) + L"必须是 " + std::to_wstring(lows[i]) +
                    L"–" + std::to_wstring(highs[i]) + L" 范围内的整数。";
            return false;
        }
    }
    config = parsed;
    return true;
}

std::filesystem::path GetSettingsDirectory() {
    PWSTR path = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path);
    if (FAILED(hr)) {
        CoTaskMemFree(path);
        throw std::runtime_error("无法获取 LocalAppData 目录");
    }
    std::filesystem::path directory;
    try {
        directory = std::filesystem::path(path) / L"OxStream" / L"Sender";
    } catch (...) {
        CoTaskMemFree(path);
        throw;
    }
    CoTaskMemFree(path);
    return directory;
}

Config LoadConfig(const std::filesystem::path& directory) {
    const auto path = directory / L"settings.ini";
    Config config;
    // 单个无效字段不影响其他有效字段的恢复。
    for (int i = 0; i < 5; ++i) {
        wchar_t value[128]{};
        const DWORD length = GetPrivateProfileStringW(L"Sender", kKeys[i], L"", value,
                                                       static_cast<DWORD>(std::size(value)), path.c_str());
        if (length == 0 || length >= std::size(value) - 1) continue;
        auto fields = ToFields(config);
        fields[i] = value;
        Config candidate;
        int invalid = 0;
        std::wstring error;
        if (ParseConfig(fields, candidate, invalid, error)) config = candidate;
    }
    return config;
}

bool SaveConfig(const std::filesystem::path& directory, const Config& config) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return false;
    const auto fields = ToFields(config);
    const auto path = directory / L"settings.ini";
    bool saved = true;
    for (int i = 0; i < 5; ++i) {
        if (!WritePrivateProfileStringW(L"Sender", kKeys[i], fields[i].c_str(), path.c_str())) saved = false;
    }
    return saved;
}

} // namespace OxStream::SenderApp
