# OxStream

Windows x64 原生 Win32 GUI 发送程序。直接复用 Host 的 DXGI 采集、LZ4 压缩、UDP 发送源码及共享协议，不依赖 CUDA、TensorRT、鼠标 DLL 或 Web 面板。

## 构建与发布

使用 Windows SDK、CMake 3.28+、Ninja 和 MSVC x64 工具链。在 CLion 中选用 x64 Visual Studio 工具链；命令行请使用 x64 Native Tools Command Prompt。

仓库根工程已包含 `OxStream` 目标。在 CLion 中选择该目标即可。也可以从本目录单独构建，无需配置 Client 的 GPU 依赖：

```powershell
cd OxStream
cmake --preset windows-x64
cmake --build --preset release
```

发布 `OxStream/build_x64/OxStream.exe`。OxStream 及其专属 LZ4 库静态链接 MSVC 运行库；发布无需携带第三方 DLL，也不要求安装 VC++ 运行库。仍依赖 Windows 自带的 D3D11、DXGI、WinSock 等组件及支持桌面复制的显卡驱动。面向 Windows 10 1703 及以上版本。

从仓库根目录构建时，使用 `cmake --build build_x64 --target OxStream`，产物位于 `build_x64/OxStream/OxStream.exe`。整个仓库仍为 Synapse-X，只有本模块使用 OxStream 名称。

## 使用

1. 在配套新版 Android-OriginX 中选择 OxStream 输入并启动，默认监听 UDP 8888，确保手机与发送电脑网络可达。
2. 双击 OxStream，填写手机的单播 IPv4、端口、ROI 宽高和目标发送帧率。安卓接收端要求 ROI 每边为 64～640。
3. 点击“开始发送”。发送期间锁定输入；点击“停止发送”，等资源释放后即可修改参数并重新开始。
4. 关闭窗口会停止发送并退出，没有托盘驻留，也不会在下次启动时自动发送。

默认值为 `192.168.100.2:8888`、中心 ROI `416×416`、目标 `170 FPS`。端口允许 1–65535；ROI 每边允许 64–4096，且不能超过实际采集输出尺寸；帧率允许整数 1–1000。帧率只是调度目标，不是吞吐保证。

采集继续使用默认显卡上的第一个活动输出，只截取中心 ROI。映射 DXGI BGRA 暂存纹理后，在逐行复制时直接去掉 Alpha，按 B、G、R 紧密排列后压缩；没有新画面时继续发送缓存，每次发送尝试递增帧号。LZ4 加速参数、UDP 分片和失败处理保持原样，无重传；`modelId` 固定为 0，本程序没有模型选择或推理控制功能。

新版使用 BGR 魔数 `0x5342`（小端字节 `42 53`），24 字节包头及 1400 字节分片负载上限不变，保留字节写零。解压长度为 `width × height × 3`，必须与新版安卓配套升级；不提供格式切换，旧安卓及 PC Client/View 无法接收。共用采集和发送接口默认仍为 BGRA（`0x5358`），Host 行为不变。

原始像素字节数减少 25%，不代表压缩后流量或延迟也减少 25%；实际收益需同设备、同画面对照测量。

压缩使用启动时分配的双缓冲：`CompressInto` 直接写工作区，成功后交换为发送缓存，有效长度单独保存，不再经过压缩器内部缓冲复制和缓存整帧复制。失败不会覆盖上一张成功缓存；没有新画面时仍重发缓存。LZ4 acceleration=5、帧号及发送失败行为不变，Host 继续使用原 `Initialize/Compress` 接口。

窗口显示实际采集 FPS、整帧成功提交 FPS、累计失败帧及累计成功帧，每秒刷新，每次开始清零。**发送成功只表示所有分片在本机调用成功，不代表对端收到完整帧。** 无接收端时 UDP 发送仍可能显示成功。

## 配置和日志

- 成功初始化后保存参数到 `%LOCALAPPDATA%\OxStream\Sender\settings.ini`，下次逐字段恢复；缺失、非法字段回退到默认值。
- 不自动迁移旧版配置和日志；首次打开 OxStream 使用默认参数，旧数据保持原样。
- 日志位于 `%LOCALAPPDATA%\OxStream\Sender\logs\oxstream.log`，使用滚动文件，不创建控制台。
- 参数保存或日志创建失败会在界面提示，不阻止发送。初始化失败、异常或屏幕缩小至无法容纳 ROI 时会停止并提示原因。
- 停止信号可以唤醒帧间等待；正在进行的核心采集、DXGI 重建或整帧发送调用会完成后再退出，不强行终止线程。

## 手动验收

编译只确认构建和链接结果，不代表实际收图功能已验收。请自行运行并检查：

- GUI 启动、不同 DPI/跨屏移动、键盘 Tab 导航及中文显示。
- 默认参数、有效字段恢复、非法 IP/端口/尺寸/FPS、超屏幕 ROI 的错误定位。
- 连续开始/停止、1 FPS 时停止、初始化期间关闭和发送期间关闭。
- 安卓局域网收图、不同 ROI（含 65 像素奇数宽度）、不同目标端口和 FPS、静止画面的缓存发送；核对红绿蓝白、方向、填边及遮罩，分别检查 MNN/NCNN、AHB/PBO 路径。
- 对端未启动、网络中断、显示模式切换时界面可操作并能停止。
- 在没有 VC++ 运行库、CUDA、TensorRT 的 Windows x64 环境仅复制 EXE 后启动和发送。

每次开始仍从帧号 0 发送；安卓接收端在 2 秒无可用帧后解除来源绑定并清空序号状态，短时间重启发送端可能需要等待恢复。本次不增加会话或重传协议。

`tests/` 是独立 C++17 像素打包用例，不依赖 DXGI/WinSock，也不加入默认应用构建，覆盖颜色、不同 Alpha、行尾填充、多行、奇数宽度和默认 BGRA 行为。可自行使用 `cmake -S tests -B build-pixel-tests`、`cmake --build build-pixel-tests`、`ctest --test-dir build-pixel-tests --output-on-failure`。本次仅静态检查，未编译、构建或执行测试。

独立测试工程另含压缩缓冲用例，直接链接仓库已有 LZ4 C 源码，覆盖直接压缩往返、连续不同尺寸/内容、无新画面重用缓存、失败保留缓存、缓冲容量及原压缩接口。用例未执行；实际延迟需与同设备、同画面基线对照，不以发送 FPS 代替端到端耗时。
