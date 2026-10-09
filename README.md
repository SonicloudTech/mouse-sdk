# 声云智能鼠标 SDK（uMouse SDK）

uMouse SDK · SoniCloud Smart Mouse SDK

语言 / Languages：**简体中文 + English 同页双语（bilingual on one page）** · 纯英文版 English-only：[README.en.md](README.en.md)

[![release](https://img.shields.io/badge/release-v0.4.20-blue)](https://github.com/SonicloudTech/mouse-sdk/releases) ![platforms](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey) ![arch](https://img.shields.io/badge/arch-x86__64%20%7C%20ARM64-orange) ![ABI](https://img.shields.io/badge/ABI-pure%20C-9cf) [![samples](https://img.shields.io/badge/demo%20samples-MIT-green)](LICENSE)

> 一套**跨系统、跨架构**的智能鼠标接入 SDK：Windows / macOS / Linux（含统信 UOS、麒麟 Kylin、方德 FangDe 等国产系统）× x86_64 / ARM64，USB（HID）与 BLE（蓝牙低功耗）双通道，纯 C ABI。
>
> A **cross-platform, cross-architecture** smart mouse SDK: Windows / macOS / Linux (incl. Chinese domestic systems UOS, Kylin, FangDe) × x86_64 / ARM64, dual-channel USB (HID) & BLE (Bluetooth Low Energy), pure C ABI.

声云智能鼠标 SDK 是面向硬件厂商、软件开发者和行业集成商的智能鼠标接入方案：一套统一的 C 接口与事件协议，覆盖设备发现连接、按键/事件/音频数据搬运的全部链路。本项目提供：

*The SoniCloud Smart Mouse SDK is a smart-mouse integration solution for hardware vendors, software developers and industry integrators: one unified C interface and event protocol covering device discovery, connection, and key / event / audio data delivery. This repository provides:*

| 资源 Resource | 说明 Description |
|---|---|
| 平台交付包 Packages | Windows x64 / macOS ARM64 二进制包（`packages/`，当前 0.4.20），其余平台组合联系商务<br>*Windows x64 / macOS ARM64 binaries (`packages/`, currently 0.4.20); other combinations via sales* |
| 接口文档 Documentation | 中文：[docs/第三方接入说明.md](docs/第三方接入说明.md) · English：[docs/Integration-Guide.en.md](docs/Integration-Guide.en.md)（v2.2：API、事件 JSON 协议、C/C++/C#/Python/Electron 集成、FAQ）<br>*v2.2 — API reference, JSON event protocol, per-language integration guides, FAQ; Chinese and English versions linked* |
| 头文件 Header | `include/umouse_sdk.h` —— 唯一对外头文件，纯 C ABI<br>*the single public header, pure C ABI* |
| 示例源码 Samples | `demo/`（最小接入 / 事件打印 / 多设备混合模式，纯 C）<br>*minimal / event printing / multi-device mixed mode, plain C* |
| 许可声明 Notices | `THIRD_PARTY_LICENSES.txt` 及随附许可证全文<br>*third-party notices with full license texts* |

## 能做什么 · What It Does

- **设备扫描与连接** · *Device discovery & connection*：USB 2.4G 接收器即插即用；BLE 扫描与自动接管；双通道自动偏好（USB 优先）与运行时切换，设备身份（SN）跨通道连续。<br>*Plug-and-play USB 2.4G dongles; BLE scanning with automatic takeover; automatic channel preference (USB first) with runtime switching — device identity (SN) stays stable across channels.*
- **多设备并发** · *Multi-device concurrency*：同型号多台共用一条规则（默认每规则 4 台、全局 8 台，可配），每台设备独立解析管线，互不影响。<br>*Multiple units of the same model share one rule (4 per rule / 8 global by default, configurable); each device gets an independent parsing pipeline.*
- **按键事件** · *Key events*：语音 / 翻译 / M 键 / AI / 截图键的按下、抬起、长按、单击；未接入键型以原始键值透传（`code`），**不做任何业务映射**。<br>*Speech / translation / M / AI / capture keys with down, up, hold and click actions; unknown key types passed through as raw codes (`code`) — no business mapping is applied.*
- **离散事件（JSON）** · *Discrete events (JSON)*：设备信息（SN / 电量 / 会议状态）、DPI 广播、会议创建/销独占调度、统一告警信封（配对坏态 / 设备数 / BLE 链路数预警）。<br>*Device info (SN / battery / meeting state), DPI broadcasts, meeting lifecycle with global exclusivity, and a unified warning envelope (pairing-broken / device-count / BLE-link-count alerts).*
- **音频输出** · *Audio output*：内置 SBC / ADPCM 等解码为 PCM（16 kHz / 16 bit / 单声道），或透传原始编码块由调用方自解码。<br>*Built-in SBC / ADPCM decoding delivers PCM (16 kHz / 16-bit / mono), or passes raw encoded blocks through for your own decoder.*
- **RAW 模式** · *RAW mode*：自定义硬件协议接入——自定义握手、设备匹配、SN 提取回调 + `um_write_raw` 双向透传。<br>*Integrate hardware with custom protocols — custom handshake, device-matching and SN-extraction callbacks, plus `um_write_raw` for bidirectional passthrough.*
- **诊断** · *Diagnostics*：进程级崩溃记录（不含业务数据）、内部日志开关、队列健康上报。<br>*Process-level crash recorder (technical data only), internal logging switch, queue-health reporting.*

## 平台支持（跨系统 × 跨架构）· Platform Support (Cross-OS × Cross-Arch)

| 系统 OS | 架构 Arch | USB | BLE | 交付包 Package |
|---|---|---|---|---|
| Windows 10 / 11 | x64 | ✅ | ✅ | 本仓库 `packages/` · *this repo* |
| macOS 11+ | arm64（Apple Silicon） | ✅ | ✅ | 本仓库 `packages/` · *this repo* |
| 统信 UOS · UOS (UnionTech) | x86_64 / ARM64 | ✅ | ✅ | 联系商务 · Contact sales |
| 麒麟 Kylin | x86_64 / ARM64 | ✅ | ✅ | 联系商务 · Contact sales |
| 方德 FangDe | x86_64 / ARM64 | ✅ | ✅ | 联系商务 · Contact sales |
| 通用 Linux · Generic Linux | x86_64 | ✅ | ✅ | 联系商务 · Contact sales |
| macOS（Intel） | x86_64 | ✅ | ✅ | 联系商务 · Contact sales |

> SDK **源码级跨平台跨架构**（CMake 工程，Windows / macOS / Linux 三系，各架构独立构建）：同一套 C ABI 与 JSON 事件协议在全部平台一致，集成代码无需按平台改写；二进制按「系统 × 架构」构建、不互通。本仓库当前发布 Windows x64 与 macOS ARM64 两个包，其余平台组合为既有支持项，交付包联系商务获取。
>
> *The SDK is cross-platform and cross-architecture at the source level (CMake project; Windows / macOS / Linux; built per architecture): the same C ABI and JSON event protocol on every platform, so integration code never changes per platform; binaries are built per OS × architecture and are not interchangeable. This repository currently ships Windows x64 and macOS ARM64 packages; the remaining combinations are supported as well — contact sales for their packages.*

## 仓库结构 · Repository Layout

```
├── README.md                  本文件（中英双语 bilingual）
├── README.en.md               纯英文版 English-only
├── docs/第三方接入说明.md       完整接入文档 v2.2（中文）
├── docs/Integration-Guide.en.md  完整接入文档 v2.2（English）
├── include/umouse_sdk.h       对外唯一头文件（纯 C ABI）
├── demo/                      C 示例（demo.c 最小接入 / demo3.c 多设备）
├── packages/                  平台交付包 platform packages
│   ├── uMouseSdk_0.4.20_Windows_10-11_x64.zip
│   └── uMouseSdk_0.4.20_macOS_11_arm64.zip
├── img/                       联系二维码 contact QR
├── LICENSE                    MIT（demo 示例代码 samples）
├── THIRD_PARTY_LICENSES.txt   第三方许可声明 third-party notices
├── COPYING.LGPLv2.1           LGPL v2.1 全文（FFmpeg）
└── LICENSE.hidapi-bsd         HIDAPI BSD-3-Clause 全文
```

## 五分钟上手 · Quick Start

解压对应平台交付包，在仓库根目录编译运行 `demo/demo.c`：
*Unzip the package for your platform and build the minimal sample `demo/demo.c` at the repository root:*

```bat
REM Windows（MSVC 开发者命令行 / developer prompt）
cl /I uMouseSdk\include demo\demo.c /Fe:demo.exe /link /LIBPATH:uMouseSdk uMouseSdk.lib
set PATH=%CD%\uMouseSdk;%PATH%
demo.exe
```

```bash
# macOS
cc -IuMouseSdk/include demo/demo.c -LuMouseSdk -luMouseSdk -o demo && ./demo
```

核心调用顺序：**注册回调 → `um_sdk_init()` → 业务处理 → `um_sdk_close()`**。SDK 只负责把硬件数据忠实搬运出来（按键事件 / JSON 离散事件 / PCM 音频三线回调），M 键做什么、音频如何识别等业务逻辑全部由调用方实现。完整步骤与各语言接入见 [docs/第三方接入说明.md](docs/第三方接入说明.md)。

*Core call order: **register callbacks → `um_sdk_init()` → run your business logic → `um_sdk_close()`**. The SDK faithfully delivers hardware data (key events / JSON discrete events / PCM audio on three callback lines); what the M key does and how audio gets recognized is entirely up to you. For the full walkthrough see the documentation.*

## 集成方式 · Integration Languages

纯 C ABI，主流语言直接调用，无需私有运行时：
*Pure C ABI — callable from mainstream languages with no proprietary runtime:*

| 语言 Language | 方式 How |
|---|---|
| C / C++ | 头文件 + 导入库直接链接（MSVC / MinGW / clang / gcc）<br>*link directly against the header + import library* |
| C# (.NET) | P/Invoke，无需第三方库<br>*P/Invoke, no third-party dependencies* |
| Python | 标准库 ctypes<br>*standard library ctypes* |
| Electron / Node.js | koffi 等 FFI 库<br>*koffi or other FFI libraries* |

## 适用场景 · Use Cases

- 语音输入法、翻译工具、会议记录等办公效率软件的硬件入口。<br>*Hardware input for voice input, translation tools and meeting-note productivity software.*
- 硬件厂商基于自有协议的二次开发（RAW 模式自定义接入）。<br>*Hardware vendors building on their own protocols (RAW-mode custom integration).*
- 行业集成商在金融、政务等信创终端（UOS / 麒麟 / 方德，x86 / ARM64）上的跨平台部署。<br>*Industry integrators deploying on Xinchuang (Chinese domestic IT) terminals — UOS / Kylin / FangDe, x86 / ARM64 — with one codebase.*

## 硬件适配与技术支持 · Hardware & SDK Access

如需获取信创（UOS / 麒麟 / 方德）与其他架构的交付包、硬件规格、样机、协议完整版、企业微信二维码或技术支持资料，请联系 **安徽声云**：[sinicloud.com](https://www.sinicloud.com/)，开放平台 [open.sinicloud.com](https://open.sinicloud.com/)。咨询时请说明目标平台、预计数量和应用场景。

*For packages of Chinese domestic OS platforms (UOS / Kylin / FangDe) and other architectures, hardware specs, evaluation units, the full protocol, a WeChat Work contact QR code, or technical support materials, contact **SoniCloud** (安徽声云智能科技有限公司): [sinicloud.com](https://www.sinicloud.com/), open platform [open.sinicloud.com](https://open.sinicloud.com/). Please mention your target platform, expected volume and use case.*

<img src="img/企业微信.png" alt="企业微信 WeChat Work" width="200" style="max-width: 100%; height: auto;">

## 第三方组件与许可 · Third-Party Components & Licensing

SDK 二进制动态链接 FFmpeg（LGPL-2.1+，独立动态库形式随附，可由最终用户自行替换）并静态编入 HIDAPI（BSD-3-Clause）；macOS 包为按实际使用的音频解码器裁剪的最小 FFmpeg 构建（无 GPL 组件，构建配置见包内 README）。署名与义务履行方式见 `THIRD_PARTY_LICENSES.txt` 与随附许可证全文。

*The SDK binaries dynamically link FFmpeg (LGPL-2.1+, shipped as standalone dynamic libraries that end users may replace) and statically embed HIDAPI (BSD-3-Clause). The macOS package ships a minimal audio-only FFmpeg build containing no GPL components (build configuration in the package README). Attribution and obligations are detailed in `THIRD_PARTY_LICENSES.txt` and the accompanying license texts.*

## 许可与使用边界 · License & Scope of Use

仓库内 `demo/` 示例代码以 MIT License 发布。SDK 二进制库、设备私有协议和部分文档可能包含声云或厂商专有内容，不等同于全部开源，商用范围以随包说明及双方商务协议为准。SDK 负责把硬件数据忠实搬运出来，不替代应用侧的语音识别、业务映射等职责。请勿将私有协议用于未获授权的硬件或产品。

*Sample code under `demo/` is released under the MIT License. SDK binaries, the device's proprietary protocol and some documents may contain proprietary content of SoniCloud or its partners and are not open source as a whole; the commercial scope is governed by the accompanying notices and the commercial agreement between the parties. The SDK faithfully delivers hardware data and does not replace application-side responsibilities such as speech recognition or business mapping. Do not use the proprietary protocol for unauthorized hardware or products.*

## 反馈与问题 · Feedback & Issues

请附带 `um_sdk_init(1)` 打开的日志、目标平台与 SDK 版本号（`um_sdk_version()`），通过仓库 Issues 反馈。

*Please attach the logs enabled via `um_sdk_init(1)`, your target platform and the SDK version (`um_sdk_version()`), then open an issue.*

---

关键词：声云智能鼠标、智能鼠标 SDK、uMouse SDK、语音鼠标、AI 鼠标、跨平台、跨架构、信创、麒麟、统信 UOS、方德 FangDe

*Keywords: smart mouse SDK, mouse SDK, uMouse SDK, SoniCloud, voice mouse, AI mouse, BLE SDK, HID SDK, USB SDK, cross-platform, cross-architecture, Windows, macOS, Linux, ARM64, Xinchuang, UOS, Kylin, FangDe*
