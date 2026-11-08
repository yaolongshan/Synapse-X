# OxStream 采集读回顺序 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** OxStream 取消显式 Flush，完成 CPU 读取后释放桌面帧，供用户与旧版对照。

**Architecture:** `CaptureReadbackMode` 明确选择顺序，默认保持 Host 原调用行为。局部 `DxgiReadbackScope` 管理成功获取的帧及成功映射的暂存纹理，异常时按 Unmap、ReleaseFrame 的顺序清理。

**Tech Stack:** Windows DXGI/D3D11、C++17、现有独立 CMake 用例。

**Spec:** [已确认的设计](../specs/2026-11-08-capture-readback-order-design.md)

## Global Constraints

- 在现有发送端工作区修改；保留 `host/.run/SynapseX_Host.run.xml`。
- 仅补充用例源码和静态复查；不自动编译、构建、运行、测试、提交或推送。
- Host 默认顺序、Android、协议、打包、压缩、重发和帧率调度保持不变。
- 保留 `capture_ms` 整段计时，不增加逐帧计时和日志。

## Review Focus

- Acquire 成功但纹理查询或 Map 失败：帧释放一次，未映射的资源不 Unmap。
- Map 成功后打包异常：先 Unmap 后 ReleaseFrame，异常继续传播。
- Host 提前释放：后续正常结束或失败不能重复 ReleaseFrame。
- 正常返回及下一帧：没有跨调用持有的帧或映射。
- 停止、模式切换和重建：沿用原循环和错误处理，真实驱动行为由用户验收。

### Task 1: 清理作用域与用例

**Files:** `host/include/DxgiReadbackScope.h`、`OxStream/tests/dxgi_readback_scope_tests.cpp`、`OxStream/tests/CMakeLists.txt`。

**Interfaces:** `detail::DxgiReadbackScope<Duplication, Context, Texture>` 接受三个非空借用指针，仅在成功 Acquire 后构造；提供 `MarkMapped()`、`Unmap()`、`ReleaseFrame()`，禁止复制和移动，析构先 Unmap 后 ReleaseFrame。

- [x] 先补映射前退出、映射后异常、显式清理不重复、提前释放后成功/失败、连续两帧用例源码。
- [x] 实现无堆分配的局部作用域，按独立状态配对清理。
- [x] 注册独立测试目标，静态检查边界和参数，不运行。

### Task 2: 接入采集顺序并记录对照步骤

**Files:** `host/include/DxgiCapturer.h`、`host/src/DxgiCapturer.cpp`、`OxStream/src/SenderSession.cpp`、`OxStream/README.md`。

**Interfaces:** `CaptureFrame(outBuffer, format, CaptureReadbackMode mode = CaptureReadbackMode::FlushBeforeRelease)`；另一个枚举值为 `MapBeforeRelease`，仅 OxStream 显式使用。

- [x] 默认路径保持 Flush、ReleaseFrame、Map 顺序，新路径在 Unmap 后 ReleaseFrame。
- [x] OxStream 选择新路径，会话日志追加识别标记，沿用整段 `capture_ms`。
- [x] 文档补充真实语义、清理范围、旧/新/旧对照方法。
- [x] 检查调用点、diff、清理分支、原配置哈希，进行独立静态复查。

## 实施记录

- 基线：`1e503dbf87fb690403302e562813a9e0e35de253`；只有发送端运行配置存在既有本地修改。
- 已获用户对上述范围的确认，直接在现有工作区实施，不重复申请批准或创建新工作树。
- 用户约束优先于技能的执行测试和提交步骤；只补源码并静态检查，不宣称测试通过。
- Task 1/2 共享作用域和枚举接口，先实现清理再接入；真实驱动顺序留作人工验收。
- Task 1/2 代码与文档已完成；`git diff --check` 未发现空白错误，清理顺序、默认参数和全部调用点已静态核对。用例未编译或执行。
- 独立只读复核未发现需要修复的问题；实际 MSVC 编译、真实驱动时序、收益和模式切换恢复未作运行验证，交由用户实测。
- 原运行配置 SHA256 保持 `CEE67CC3D39949CBF5709A165E0583977000AE9F95A2B7A25CBCE98742078CC3`，Android 工作区未修改。
