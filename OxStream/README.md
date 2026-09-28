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

1. 在接收电脑运行现有 `SynapseX_View.exe [端口]`，默认监听 UDP 8888。确保接收端防火墙允许该端口。View 与 Client 不要同时占用同一端口。
2. 双击 OxStream，填写接收电脑的单播 IPv4、端口、ROI 宽高和目标发送帧率。
3. 点击“开始发送”。发送期间锁定输入；点击“停止发送”，等资源释放后即可修改参数并重新开始。
4. 关闭窗口会停止发送并退出，没有托盘驻留，也不会在下次启动时自动发送。

默认值为 `192.168.100.2:8888`、中心 ROI `416×416`、目标 `170 FPS`。端口允许 1–65535；ROI 每边允许 64–4096，且不能超过实际采集输出尺寸；帧率允许整数 1–1000。帧率只是调度目标，不是吞吐保证。

采集继续使用默认显卡上的第一个活动输出，只截取中心 ROI。没有新画面时继续发送缓存；每次发送尝试递增帧号。LZ4 加速参数、UDP 分片和失败处理均保持原样，无重传。协议 `modelId` 固定为 0，View 可直接显示；本程序没有模型选择或推理控制功能。

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
- 局域网 View 收图、不同 ROI、不同目标端口和 FPS、静止画面的缓存发送。
- 对端未启动、网络中断、显示模式切换时界面可操作并能停止。
- 在没有 VC++ 运行库、CUDA、TensorRT 的 Windows x64 环境仅复制 EXE 后启动和发送。

现有核心源码及协议不在本次修改范围内，原有边界和限制仍然存在。

特别是每次开始会按约定从帧号 0 重新发送：如果上次停止前接收端留下了较大帧号的残缺帧，现有接收器可能暂时拒收重新开始后的较小帧号。遇到再次开始后不出图，可重启 View 清空重组状态。本次不修改接收器或增加会话/重传协议。
