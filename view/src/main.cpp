// SynapseX_View — 收 Host 的 LZ4/UDP 画面并 D3D11 显示（测延迟用）
//   SynapseX_View.exe [port]     默认 8888
// 不要和 SynapseX_Client 同时开（抢同一端口）。Host 照常发。

#include "UdpReceiver.h"
#include "Log.h"
#include "PacketHeader.h"

#include <winsock2.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

std::atomic<uint8_t> g_targetModelId{0};

using Microsoft::WRL::ComPtr;

static constexpr wchar_t kWndClass[] = L"SynapseX_View";

static const char* kShader = R"(
Texture2D tex : register(t0);
SamplerState samp : register(s0);
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VSOut vs_main(uint id : SV_VertexID) {
    float2 uv = float2((id << 1) & 2, id & 2);
    VSOut o;
    o.uv = uv;
    o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
float4 ps_main(VSOut i) : SV_Target {
    return tex.Sample(samp, i.uv);
}
)";

struct Presenter {
    HWND hwnd = nullptr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;
    ComPtr<IDXGISwapChain1> swap;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11ShaderResourceView> srv;
    UINT texW = 0, texH = 0;
    UINT swapW = 0, swapH = 0;
    bool tearing = false;
    bool ok = false;

    bool Init(HWND h, int w, int hgt) {
        hwnd = h;
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL fl;
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            nullptr, 0, D3D11_SDK_VERSION,
            device.GetAddressOf(), &fl, ctx.GetAddressOf());
        if (FAILED(hr)) {
            SX_LOG_ERROR("[View] D3D11CreateDevice 失败 HRESULT=0x{:08X}", (unsigned)hr);
            return false;
        }

        ComPtr<IDXGIDevice> dxgiDev;
        device.As(&dxgiDev);
        ComPtr<IDXGIAdapter> adapter;
        dxgiDev->GetAdapter(adapter.GetAddressOf());
        ComPtr<IDXGIFactory2> factory;
        adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));

        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(factory.As(&factory5))) {
            BOOL allow = FALSE;
            if (SUCCEEDED(factory5->CheckFeatureSupport(
                    DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow)))) {
                tearing = allow == TRUE;
            }
        }

        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = (UINT)w;
        desc.Height = (UINT)hgt;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Flags = tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        hr = factory->CreateSwapChainForHwnd(
            device.Get(), hwnd, &desc, nullptr, nullptr, swap.GetAddressOf());
        if (FAILED(hr)) {
            SX_LOG_ERROR("[View] CreateSwapChainForHwnd 失败 HRESULT=0x{:08X}", (unsigned)hr);
            return false;
        }
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        swapW = desc.Width;
        swapH = desc.Height;
        if (!CreateRtv()) return false;
        if (!CreatePipeline()) return false;
        ok = true;
        return true;
    }

    bool CreateRtv() {
        rtv.Reset();
        ComPtr<ID3D11Texture2D> back;
        HRESULT hr = swap->GetBuffer(0, IID_PPV_ARGS(back.GetAddressOf()));
        if (FAILED(hr)) return false;
        hr = device->CreateRenderTargetView(back.Get(), nullptr, rtv.GetAddressOf());
        return SUCCEEDED(hr);
    }

    bool CreatePipeline() {
        ComPtr<ID3DBlob> vsBlob, psBlob, err;
        HRESULT hr = D3DCompile(kShader, strlen(kShader), nullptr, nullptr, nullptr,
                                "vs_main", "vs_5_0", 0, 0, vsBlob.GetAddressOf(), err.GetAddressOf());
        if (FAILED(hr)) {
            SX_LOG_ERROR("[View] VS 编译失败");
            return false;
        }
        hr = D3DCompile(kShader, strlen(kShader), nullptr, nullptr, nullptr,
                        "ps_main", "ps_5_0", 0, 0, psBlob.GetAddressOf(), err.GetAddressOf());
        if (FAILED(hr)) {
            SX_LOG_ERROR("[View] PS 编译失败");
            return false;
        }
        if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                              nullptr, vs.GetAddressOf()))) {
            return false;
        }
        if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                             nullptr, ps.GetAddressOf()))) {
            return false;
        }

        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        return SUCCEEDED(device->CreateSamplerState(&sd, sampler.GetAddressOf()));
    }

    bool ResizeSwap(UINT w, UINT h) {
        if (!swap || w == 0 || h == 0) return true;
        if (w == swapW && h == swapH) return true;
        ctx->OMSetRenderTargets(0, nullptr, nullptr);
        rtv.Reset();
        HRESULT hr = swap->ResizeBuffers(
            0, w, h, DXGI_FORMAT_UNKNOWN,
            tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
        if (FAILED(hr)) {
            SX_LOG_ERROR("[View] ResizeBuffers 失败 HRESULT=0x{:08X}", (unsigned)hr);
            return false;
        }
        swapW = w;
        swapH = h;
        return CreateRtv();
    }

    bool EnsureTex(UINT w, UINT h) {
        if (tex && texW == w && texH == h) return true;
        tex.Reset();
        srv.Reset();
        texW = w;
        texH = h;

        D3D11_TEXTURE2D_DESC td = {};
        td.Width = w;
        td.Height = h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DYNAMIC;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        HRESULT hr = device->CreateTexture2D(&td, nullptr, tex.GetAddressOf());
        if (FAILED(hr)) return false;
        return SUCCEEDED(device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf()));
    }

    void UploadBgra(const uint8_t* pixels, UINT w, UINT h) {
        if (!EnsureTex(w, h) || !tex) return;
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(ctx->Map(tex.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
        const UINT srcPitch = w * 4;
        if (mapped.RowPitch == srcPitch) {
            memcpy(mapped.pData, pixels, (size_t)srcPitch * h);
        } else {
            auto* dst = static_cast<uint8_t*>(mapped.pData);
            for (UINT y = 0; y < h; ++y) {
                memcpy(dst + y * mapped.RowPitch, pixels + y * srcPitch, srcPitch);
            }
        }
        ctx->Unmap(tex.Get(), 0);
    }

    void Draw() {
        if (!rtv) return;
        const float clear[4] = {0, 0, 0, 1};
        ctx->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
        ctx->ClearRenderTargetView(rtv.Get(), clear);

        D3D11_VIEWPORT vp = {};
        vp.Width = (float)swapW;
        vp.Height = (float)swapH;
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);

        if (srv) {
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(vs.Get(), nullptr, 0);
            ctx->PSSetShader(ps.Get(), nullptr, 0);
            ctx->PSSetShaderResources(0, 1, srv.GetAddressOf());
            ctx->PSSetSamplers(0, 1, sampler.GetAddressOf());
            ctx->Draw(3, 0);
            ID3D11ShaderResourceView* none = nullptr;
            ctx->PSSetShaderResources(0, 1, &none);
        }

        UINT presentFlags = tearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
        swap->Present(0, presentFlags);
    }
};

static Presenter g_pres;
static bool g_running = true;

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_SIZE:
        if (g_pres.ok && w != SIZE_MINIMIZED) {
            g_pres.ResizeSwap(LOWORD(l), HIWORD(l));
        }
        return 0;
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(h, m, w, l);
    }
}

static void FitClientSize(HWND hwnd, int w, int h) {
    RECT rc = {0, 0, w, h};
    DWORD style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
    DWORD ex = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&rc, style, FALSE, ex);
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER);
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SynapseX::Log::Initialize("view");

    uint16_t port = (argc > 1) ? static_cast<uint16_t>(std::atoi(argv[1])) : 8888;
    SX_LOG_INFO("[View] 监听 0.0.0.0:{} — 不要和 SynapseX_Client 同时开。Host 先发画面。", port);

    SynapseX::UdpReceiver receiver;
    if (!receiver.Initialize(port)) {
        SX_LOG_CRITICAL("[View] UdpReceiver 初始化失败");
        return 1;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWndClass;
    RegisterClassExW(&wc);

    const int initW = 416, initH = 416;
    HWND hwnd = CreateWindowExW(
        0, kWndClass, L"SynapseX View",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, initW, initH,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) {
        SX_LOG_CRITICAL("[View] CreateWindow 失败");
        return 1;
    }
    FitClientSize(hwnd, initW, initH);
    ShowWindow(hwnd, SW_SHOW);

    RECT cr = {};
    GetClientRect(hwnd, &cr);
    if (!g_pres.Init(hwnd, cr.right - cr.left, cr.bottom - cr.top)) {
        return 1;
    }

    std::vector<uint8_t> frame;
    uint32_t frameId = 0;
    int lastW = 0, lastH = 0;

    using Clock = std::chrono::high_resolution_clock;
    auto hudAt = Clock::now();
    uint64_t framesAtHud = 0;
    double sumRecvMs = 0.0;
    int recvSamples = 0;
    uint32_t hudId = 0;

    while (g_running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_running) break;

        auto t0 = Clock::now();
        bool got = receiver.TryReceive(frame, frameId);
        auto t1 = Clock::now();
        const double recvMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        if (got) {
            int w = receiver.GetLastFrameWidth();
            int h = receiver.GetLastFrameHeight();
            if (w > 0 && h > 0 && !frame.empty()) {
                if (w != lastW || h != lastH) {
                    FitClientSize(hwnd, w, h);
                    lastW = w;
                    lastH = h;
                    SX_LOG_INFO("[View] ROI {}x{}", w, h);
                }
                g_pres.UploadBgra(frame.data(), (UINT)w, (UINT)h);
            }
            sumRecvMs += recvMs;
            recvSamples++;
            hudId = frameId;
        }

        g_pres.Draw();

        auto now = Clock::now();
        if (now - hudAt >= std::chrono::seconds(1)) {
            uint64_t total = receiver.GetTotalFrames();
            int fps = (int)(total - framesAtHud);
            double avgRecv = recvSamples > 0 ? sumRecvMs / recvSamples : 0.0;
            wchar_t title[256];
            swprintf(title, 256,
                     L"SynapseX View  %dx%d  %dfps  drop=%llu  recv=%.2fms  id=%u",
                     lastW, lastH, fps,
                     (unsigned long long)receiver.GetTotalDropped(),
                     avgRecv, hudId);
            SetWindowTextW(hwnd, title);
            framesAtHud = total;
            sumRecvMs = 0.0;
            recvSamples = 0;
            hudAt = now;
        }
    }

    return 0;
}
