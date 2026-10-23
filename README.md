# uMouse SDK — USB HID / BLE 智能鼠标接入

**简体中文** | [English](README.en.md)

[![release](https://img.shields.io/badge/release-v0.4.20-blue)](https://github.com/SonicloudTech/mouse-sdk/releases/tag/v0.4.20) ![ABI](https://img.shields.io/badge/ABI-pure%20C-9cf) [![samples](https://img.shields.io/badge/demo%20samples-MIT-green)](LICENSE)

**声云智能鼠标 SDK（SoniCloud Smart Mouse SDK）**通过统一的 C ABI 接入 USB HID 与 BLE 设备，提供设备发现、连接管理、按键事件、JSON 设备信息和 PCM 音频，面向硬件厂商、应用开发者及行业集成商。可从 C/C++、C#、Python、Electron / Node.js 调用。

需要兼容的声云硬件，或按 RAW 模式完成适配的自有硬件；普通鼠标不能仅凭安装 SDK 获得这些能力。仓库提供**二进制 SDK、公开头文件、文档和 MIT 示例代码**，SDK 二进制及私有协议的使用范围见[许可说明](#license-scope)。

## 下载与开发入口

| 资源 | 入口 |
|---|---|
| Windows 10 / 11 x64 · SDK 0.4.20 | [下载 Windows ZIP](https://github.com/SonicloudTech/mouse-sdk/releases/download/v0.4.20/uMouseSdk_0.4.20_Windows_10-11_x64.zip) |
| macOS 11+ ARM64 · SDK 0.4.20 | [下载 Apple Silicon ZIP](https://github.com/SonicloudTech/mouse-sdk/releases/download/v0.4.20/uMouseSdk_0.4.20_macOS_11_arm64.zip) |
| 版本说明与全部发布 | [GitHub Releases](https://github.com/SonicloudTech/mouse-sdk/releases) |
| 完整接入文档 v2.2 | [中文指南](docs/第三方接入说明.md) · [English guide](docs/Integration-Guide.en.md) |
| C 示例与公共接口 | [按场景选示例](#examples) · [umouse_sdk.h](include/umouse_sdk.h) |
| Linux / UOS / 麒麟 / 方德等平台包、兼容硬件与样机 | [获取方式与技术支持](#hardware-access) |

## 快速开始

1. 获取本仓库的示例源码：

   ```sh
   git clone https://github.com/SonicloudTech/mouse-sdk.git
   cd mouse-sdk
   ```

2. 下载上表中对应系统和架构的 ZIP，解压到仓库根目录，确认存在 `uMouseSdk/include/umouse_sdk.h`，且 SDK 动态库位于 `uMouseSdk/` 下。仓库 [packages/](packages/) 也保留这两个平台包。
3. 连接兼容设备，在仓库根目录编译运行最小示例。Windows 使用 **x64 MSVC 开发者命令提示符**；macOS 需要 Xcode Command Line Tools。

**Windows x64：**

```bat
cl /I uMouseSdk\include demo\demo.c /Fe:umouse-demo.exe /link /LIBPATH:uMouseSdk uMouseSdk.lib
set "PATH=%CD%\uMouseSdk;%PATH%"
umouse-demo.exe
```

**macOS ARM64：**

```sh
cc -IuMouseSdk/include demo/demo.c -LuMouseSdk -luMouseSdk -Wl,-rpath,@executable_path/uMouseSdk -o umouse-demo
./umouse-demo
```

保留包内运行库与 SDK 主库的相对位置。Windows 的 DLL / VC 运行库加载说明见包内 README；macOS 首次使用 BLE 时需要允许蓝牙访问，包内已附带 FFmpeg 动态库，无需另行安装。

示例会打印 SDK 版本；设备上线后出现 `[connected]`，后续输出设备事件和音频长度，并将收到的 PCM 保存为 `out.pcm`。按回车退出。**能读到版本号只说明库已加载；设备连接和回调数据才用于确认硬件链路。**

核心调用顺序：**注册回调 → `um_sdk_init()` → 业务处理 → `um_sdk_close()`**。SDK 提供按键、设备事件和音频数据；语音识别、按键业务映射由应用实现。更多步骤与故障排查见[完整接入指南](docs/第三方接入说明.md)。

<a id="examples"></a>

## 按场景选择示例

| 场景 | 示例 |
|---|---|
| 声云设备基础接入：按键、设备信息与 PCM 音频 | [demo.c](demo/demo.c) — FULL 模式最小示例 |
| 声云设备原始帧透传，自行解析协议 | [demo2.c](demo/demo2.c) — RAW 模式、握手与通道切换 |
| 声云设备与自定义设备同时在线 | [demo3.c](demo/demo3.c) — 多规则、多设备，FULL / RAW 共存 |
| 用自定义标识、握手和解析逻辑接入外部硬件 | [demo4.c](demo/demo4.c) — 替换默认规则的完整示例 |

`demo4.c` 当前用声云设备的实际标识和协议演示接入流程；接入自有硬件时，需要替换 `HW_*` 配置、握手和解析逻辑。USB 自定义硬件的数据通道须位于所选接口的首个 HID 顶层集合或独占接口，并支持 Output Report 或 Feature Report 写入。完整前提与示例选择见[示例说明](demo/README.md)；此路径不代表任意 USB / BLE 设备无需适配即可使用。

## 能做什么

- **设备扫描与连接**：USB 2.4G 接收器即插即用；BLE 扫描与自动接管；USB 优先的通道偏好及运行时切换，同一设备的身份（SN）跨通道连续。
- **多设备并发**：同型号多台可共用规则，默认每规则 4 台、全局 8 台，可配置；每台设备独立解析。
- **按键事件**：语音 / 翻译 / M / AI / 截图键的按下、抬起、长按、单击；未接入键型以原始 `code` 透传，不做业务映射。
- **JSON 设备事件**：SN、电量、会议状态、DPI 广播、会议生命周期与独占调度，以及配对异常、设备数量、BLE 链路数量等告警。
- **音频输出**：内置 SBC / ADPCM 等解码，输出 PCM（16 kHz / 16 bit / 单声道），或透传原始编码块供应用自行解码。
- **RAW 模式**：自定义握手、设备匹配、SN 提取和协议解析，使用 `um_write_raw` 双向传输。
- **诊断**：进程级崩溃记录（不含业务数据）、内部日志开关与队列健康上报。

## 平台与架构

| 系统 | 架构 | USB | BLE | 交付方式 |
|---|---|---|---|---|
| Windows 10 / 11 | x64 | ✅ | ✅ | [公开下载](https://github.com/SonicloudTech/mouse-sdk/releases/download/v0.4.20/uMouseSdk_0.4.20_Windows_10-11_x64.zip) |
| macOS 11+ | ARM64（Apple Silicon） | ✅ | ✅ | [公开下载](https://github.com/SonicloudTech/mouse-sdk/releases/download/v0.4.20/uMouseSdk_0.4.20_macOS_11_arm64.zip) |
| 统信 UOS | x86_64 / ARM64 | ✅ | ✅ | [联系获取](#hardware-access) |
| 麒麟 Kylin | x86_64 / ARM64 | ✅ | ✅ | [联系获取](#hardware-access) |
| 方德 FangDe | x86_64 / ARM64 | ✅ | ✅ | [联系获取](#hardware-access) |
| 通用 Linux | x86_64 | ✅ | ✅ | [联系获取](#hardware-access) |
| macOS（Intel） | x86_64 | ✅ | ✅ | [联系获取](#hardware-access) |

SDK 内部源码以 CMake 工程跨系统、跨架构构建，公开接口使用统一的 C ABI 与 JSON 事件协议。**本仓库公开的是交付包和接入资源，不含 SDK 完整源码**；当前可直接下载 Windows x64 与 macOS ARM64 包，其余组合按上表联系获取。二进制按系统和架构分别构建，不能互换；Linux 各发行版也需选用匹配的包。

## 语言集成

| 语言 | 调用方式 |
|---|---|
| C / C++ | 头文件与导入库，MSVC / MinGW / clang / gcc |
| C# / .NET | P/Invoke |
| Python | 标准库 `ctypes` |
| Electron / Node.js | `koffi` 等 FFI 库 |

各语言的接入片段、回调生命周期与 API 说明见[中文接入指南](docs/第三方接入说明.md)和[英文接入指南](docs/Integration-Guide.en.md)。仓库内可独立编译的示例为上述四个 C 程序。

## 适用场景与相关项目

- 为语音输入、翻译工具和会议记录应用提供硬件输入。
- 硬件厂商通过 RAW 模式接入自有协议设备。
- 行业集成商在 UOS、麒麟、方德等 x86_64 / ARM64 信创终端部署设备接入能力。

需要让 AI 智能体操作真实鼠标，可查看 [mouse-mcp（uMouse MCP server）](https://github.com/SonicloudTech/mouse-mcp)。它通过 MCP 和兼容的 uMouse USB 硬件提供鼠标操作与语音输入入口，**目前面向 Windows x64**。本 SDK 面向应用与硬件集成；MCP 项目的平台和硬件要求以其仓库为准。

## 仓库结构

```text
├── README.md                    中文说明
├── README.en.md                 English README
├── docs/第三方接入说明.md        中文接入文档 v2.2
├── docs/Integration-Guide.en.md  英文接入文档 v2.2
├── include/umouse_sdk.h          公共 C ABI 头文件
├── demo/                        demo.c / demo2.c / demo3.c / demo4.c
├── packages/                    SDK 0.4.20：Windows x64 / macOS ARM64 ZIP
├── img/                         联系二维码
├── LICENSE                      MIT 示例代码许可
├── THIRD_PARTY_LICENSES.txt      第三方组件声明
├── COPYING.LGPLv2.1             FFmpeg 许可文本
└── LICENSE.hidapi-bsd            HIDAPI 许可文本
```

<a id="hardware-access"></a>

## 获取硬件、其他平台包与技术支持

兼容硬件规格、样机、完整协议、Linux / 信创及其他架构包、商业授权和技术支持资料，请联系**安徽声云智能科技有限公司（SoniCloud）**。请说明目标系统与架构、设备型号、预计数量和应用场景。

- 商务与技术联系：[xu_heng_yu@sinicloud.com](mailto:xu_heng_yu@sinicloud.com)
- 官网：[sinicloud.com](https://sinicloud.com/)
- 语音开放平台：[open.sinicloud.com](https://open.sinicloud.com/)

<img src="img/微信.png" alt="声云技术与商务联系微信二维码" width="200">

<a id="license-scope"></a>

## 许可与第三方组件

仓库内 `demo/` 示例代码以 [MIT License](LICENSE) 发布。SDK 二进制库、设备私有协议和部分文档可能包含声云或厂商专有内容，不等同于全部开源，商用范围以随包说明及双方商务协议为准。SDK 不替代应用侧的语音识别或业务映射。请勿将私有协议用于未获授权的硬件或产品。

SDK 二进制动态链接 FFmpeg（LGPL-2.1+，独立动态库随附，可由最终用户自行替换），并静态编入 HIDAPI（BSD-3-Clause）。macOS 包使用按实际音频解码器裁剪的最小 FFmpeg 构建，不含 GPL 组件；构建配置见包内 README。署名与使用要求见[第三方组件声明](THIRD_PARTY_LICENSES.txt)、[LGPL 文本](COPYING.LGPLv2.1)及 [HIDAPI 许可](LICENSE.hidapi-bsd)。

## 反馈问题

请通过 [GitHub Issues](https://github.com/SonicloudTech/mouse-sdk/issues) 反馈，并附上目标系统与架构、设备型号、SDK 版本（`um_sdk_version()`）、复现步骤及 `um_sdk_init(1)` 开启的相关日志。
