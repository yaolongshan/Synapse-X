#include "SenderConfig.h"
#include "SenderSession.h"
#include "SenderLog.h"
#include "Log.h"

#include <array>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace SynapseX::SenderApp {
namespace {
constexpr wchar_t kWindowClass[] = L"SynapseX_Sender_Window";
constexpr int kStartId = 101;
constexpr int kStopId = 102;
constexpr UINT_PTR kStatusTimer = 1;
constexpr int kClientWidth = 550;
constexpr int kClientHeight = 480;
constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

enum class UiState { Idle, Starting, Running, Stopping };

class Window {
public:
    Window(std::filesystem::path directory, bool logReady)
        : m_directory(std::move(directory)), m_logReady(logReady) {}
    ~Window() {
        m_session.RequestStop();
        m_session.Join();
        if (m_font) DeleteObject(m_font);
    }

    int Run(HINSTANCE instance, int show) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = kWindowClass;
        if (!RegisterClassExW(&wc)) throw std::runtime_error("窗口类注册失败");
        HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, kWindowClass,
            L"SynapseX Sender — UDP 画面发送", kWindowStyle,
            CW_USEDEFAULT, CW_USEDEFAULT, 600, 560, nullptr, nullptr, instance, this);
        if (!window) throw std::runtime_error("窗口创建失败");
        ShowWindow(window, show);
        UpdateWindow(window);
        MSG message{};
        BOOL result;
        while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            if (!IsDialogMessageW(window, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        if (result == -1) {
            m_session.RequestStop();
            m_session.Join();
            if (IsWindow(window)) DestroyWindow(window);
            return 1;
        }
        return static_cast<int>(message.wParam);
    }

private:
    struct Control { HWND window; int x, y, width, height; };
    HWND Add(const wchar_t* type, const wchar_t* text, DWORD style,
             int x, int y, int width, int height, int id = 0, DWORD exStyle = 0) {
        HWND child = CreateWindowExW(exStyle, type, text, WS_CHILD | WS_VISIBLE | style,
            0, 0, 0, 0, m_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        if (!child) throw std::runtime_error("控件创建失败");
        m_controls.push_back({child, x, y, width, height});
        return child;
    }

    void CreateControls() {
        Add(L"STATIC", L"发送参数", 0, 24, 18, 240, 24);
        constexpr const wchar_t* labels[] = {L"目标 IPv4", L"UDP 端口", L"ROI 宽度（像素）",
                                             L"ROI 高度（像素）", L"发送帧率（FPS）"};
        const auto fields = ToFields(LoadConfig(m_directory));
        for (int i = 0; i < 5; ++i) {
            Add(L"STATIC", labels[i], 0, 24, 56 + i * 36, 160, 24);
            m_inputs[i] = Add(L"EDIT", fields[i].c_str(),
                WS_TABSTOP | ES_AUTOHSCROLL | (i == 0 ? 0 : ES_NUMBER),
                200, 52 + i * 36, 320, 28, 200 + i, WS_EX_CLIENTEDGE);
            SendMessageW(m_inputs[i], EM_SETLIMITTEXT, i == 0 ? 63 : 10, 0);
        }
        m_start = Add(L"BUTTON", L"开始发送", WS_TABSTOP | BS_DEFPUSHBUTTON,
                      200, 240, 150, 34, kStartId);
        m_stop = Add(L"BUTTON", L"停止发送", WS_TABSTOP | BS_PUSHBUTTON,
                     370, 240, 150, 34, kStopId);
        m_stateText = Add(L"STATIC", L"状态：已停止", 0, 24, 292, 496, 24);
        m_statsText = Add(L"STATIC", L"采集 FPS：0.0    发送 FPS：0.0\r\n失败帧：0    累计成功帧：0",
                          0, 24, 320, 496, 48);
        Add(L"STATIC", L"发送成功仅表示本机提交成功，不代表对端收到完整画面。",
            0, 24, 374, 496, 24);
        m_errorText = Add(L"STATIC", L"", 0, 24, 410, 496, 56);
        if (!m_logReady) SetWindowTextW(m_errorText, L"日志目录不可写，本次运行无法保存日志。发送功能仍可使用。");
        RefreshEnabled();
        Layout(GetDpiForWindow(m_window), true);
        if (!SetTimer(m_window, kStatusTimer, 1000, nullptr))
            throw std::runtime_error("状态刷新定时器创建失败");
        SetFocus(m_inputs[0]);
    }

    void Layout(UINT dpi, bool resizeWindow) {
        const auto scale = [dpi](int value) { return MulDiv(value, static_cast<int>(dpi), 96); };
        HFONT font = CreateFontW(-scale(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        if (!font) throw std::runtime_error("界面字体创建失败");
        for (const auto& control : m_controls) {
            SendMessageW(control.window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            MoveWindow(control.window, scale(control.x), scale(control.y),
                       scale(control.width), scale(control.height), TRUE);
        }
        if (m_font) DeleteObject(m_font);
        m_font = font;
        if (resizeWindow) {
            RECT rect{0, 0, scale(kClientWidth), scale(kClientHeight)};
            AdjustWindowRectExForDpi(&rect, kWindowStyle, FALSE, WS_EX_CONTROLPARENT, dpi);
            SetWindowPos(m_window, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    void RefreshEnabled() {
        const bool idle = m_state == UiState::Idle && !m_closing;
        for (HWND input : m_inputs) EnableWindow(input, idle);
        EnableWindow(m_start, idle);
        EnableWindow(m_stop, !m_closing && (m_state == UiState::Starting || m_state == UiState::Running));
    }

    void ShowError(const std::wstring& error, int field = -1) {
        SetWindowTextW(m_errorText, error.c_str());
        if (field >= 0 && field < 5 && m_state == UiState::Idle && !m_closing) {
            SetFocus(m_inputs[field]);
            SendMessageW(m_inputs[field], EM_SETSEL, 0, -1);
        }
    }

    void Start() {
        if (m_state != UiState::Idle || m_closing) return;
        ConfigFields fields;
        for (int i = 0; i < 5; ++i) {
            wchar_t value[128]{};
            GetWindowTextW(m_inputs[i], value, static_cast<int>(std::size(value)));
            fields[i] = value;
        }
        int field = -1;
        std::wstring error;
        if (!ParseConfig(fields, m_config, field, error)) {
            ShowError(error, field);
            return;
        }
        m_state = UiState::Starting;
        m_observedStart = false;
        RefreshEnabled();
        SetFocus(m_stop);
        SetWindowTextW(m_stateText, L"状态：正在初始化…");
        SetWindowTextW(m_statsText, L"采集 FPS：0.0    发送 FPS：0.0\r\n失败帧：0    累计成功帧：0");
        ShowError(m_logReady ? L"" : L"本次运行无法保存日志。");
        try {
            m_session.Start(m_config, m_window);
        } catch (...) {
            m_state = UiState::Idle;
            RefreshEnabled();
            SetWindowTextW(m_stateText, L"状态：启动失败");
            ShowError(L"无法创建发送工作线程，请稍后重试。");
        }
    }

    void Stop() {
        if (m_state == UiState::Idle || m_state == UiState::Stopping) return;
        m_state = UiState::Stopping;
        m_session.RequestStop();
        RefreshEnabled();
        SetWindowTextW(m_stateText, m_closing ? L"状态：正在停止并退出…" : L"状态：正在停止…");
    }

    void Poll() {
        if (m_state == UiState::Idle) return;
        const auto status = m_session.Snapshot();
        if (status.started && !m_observedStart) {
            m_observedStart = true;
            if (!SaveConfig(m_directory, m_config))
                ShowError(L"发送已启动，但参数保存失败；下次打开可能无法恢复本次参数。");
            if (m_state == UiState::Starting) {
                m_state = UiState::Running;
                SetWindowTextW(m_stateText, L"状态：正在发送");
            }
        }
        wchar_t stats[256]{};
        swprintf_s(stats, L"采集 FPS：%.1f    发送 FPS：%.1f\r\n失败帧：%llu    累计成功帧：%llu",
                   status.done ? 0.0 : status.captureFps, status.done ? 0.0 : status.sendFps,
                   static_cast<unsigned long long>(status.failedFrames),
                   static_cast<unsigned long long>(status.sentFrames));
        SetWindowTextW(m_statsText, stats);
        if (status.done) {
            m_session.Join();
            m_state = UiState::Idle;
            if (m_closing) { DestroyWindow(m_window); return; }
            RefreshEnabled();
            SetFocus(m_start);
            SetWindowTextW(m_stateText, status.error.empty() ? L"状态：已停止" : L"状态：已停止（发生错误）");
            if (!status.error.empty()) ShowError(status.error, status.invalidField);
        }
    }

    LRESULT Handle(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_CREATE: CreateControls(); return 0;
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                if (LOWORD(wParam) == kStartId || LOWORD(wParam) == IDOK) Start();
                else if (LOWORD(wParam) == kStopId) Stop();
            }
            return 0;
        case WM_TIMER:
            if (wParam == kStatusTimer) Poll();
            return 0;
        case kSessionChanged: Poll(); return 0;
        case WM_DPICHANGED: {
            const auto* rect = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(m_window, nullptr, rect->left, rect->top,
                         rect->right - rect->left, rect->bottom - rect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            Layout(HIWORD(wParam), false);
            return 0;
        }
        case WM_CLOSE:
            m_closing = true;
            if (m_state == UiState::Idle) DestroyWindow(m_window);
            else { Stop(); RefreshEnabled(); }
            return 0;
        case WM_DESTROY:
            KillTimer(m_window, kStatusTimer);
            PostQuitMessage(0);
            return 0;
        default: return DefWindowProcW(m_window, message, wParam, lParam);
        }
    }

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
            self->m_window = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(window, message, wParam, lParam);
        try {
            return self->Handle(message, wParam, lParam);
        } catch (...) {
            // 异常不得穿过 Win32 回调边界；先结束线程，再允许窗口销毁。
            self->m_session.RequestStop();
            self->m_session.Join();
            MessageBoxW(window, L"界面处理发生异常，程序将退出。", L"SynapseX Sender", MB_OK | MB_ICONERROR);
            if (message == WM_CREATE) return -1;
            DestroyWindow(window);
            return 0;
        }
    }

    std::filesystem::path m_directory;
    bool m_logReady;
    HWND m_window = nullptr;
    HFONT m_font = nullptr;
    std::vector<Control> m_controls;
    std::array<HWND, 5> m_inputs{};
    HWND m_start = nullptr, m_stop = nullptr, m_stateText = nullptr;
    HWND m_statsText = nullptr, m_errorText = nullptr;
    SenderSession m_session;
    Config m_config;
    UiState m_state = UiState::Idle;
    bool m_closing = false;
    bool m_observedStart = false;
};
} // namespace
} // namespace SynapseX::SenderApp

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    using namespace SynapseX::SenderApp;
    int result = 1;
    try {
        const auto directory = GetSettingsDirectory();
        const bool logReady = InitializeLogger(directory);
        Window window(directory, logReady);
        result = window.Run(instance, show);
    } catch (const std::exception& error) {
        try { SX_LOG_ERROR("[Sender] 启动失败: {}", error.what()); } catch (...) {}
        MessageBoxW(nullptr, L"程序初始化失败，请检查用户配置目录权限及可用系统资源。",
                    L"SynapseX Sender", MB_OK | MB_ICONERROR);
    } catch (...) {
        MessageBoxW(nullptr, L"程序初始化发生未知错误。", L"SynapseX Sender", MB_OK | MB_ICONERROR);
    }
    SynapseX::Log::Shutdown();
    return result;
}
