# 声云智能鼠标 SDK（uMouse SDK）

> 一套**跨系统、跨架构**的智能鼠标接入 SDK：Windows / macOS / Linux（含统信 UOS、麒麟 Kylin、方德 FangDe 等国产系统）× x86_64 / ARM64，USB（HID）与 BLE（蓝牙低功耗）双通道，纯 C ABI。
>
> Cross-platform & cross-architecture smart mouse SDK (Windows / macOS / Linux, x86_64 / ARM64), USB & BLE dual-channel, pure C ABI.

声云智能鼠标 SDK 是面向硬件厂商、软件开发者和行业集成商的智能鼠标接入方案：一套统一的
C 接口与事件协议，覆盖设备发现连接、按键/事件/音频数据搬运的全部链路。本项目提供：

| 资源 | 说明 |
|---|---|
| 平台交付包 | Windows x64 / macOS ARM64 二进制包（`packages/`，当前 0.4.20），其余平台组合联系商务 |
| 接口文档 | [docs/第三方接入说明.md](docs/第三方接入说明.md)（v2.2：API、事件 JSON 协议、C/C++/C#/Python/Electron 集成、FAQ） |
| 头文件 | `include/umouse_sdk.h` —— 唯一对外头文件，纯 C ABI |
| 示例源码 | `demo/`（最小接入 / 事件打印 / 多设备混合模式，纯 C） |
| 许可声明 | `THIRD_PARTY_LICENSES.txt` 及随附许可证全文 |

## 能做什么

- **设备扫描与连接**：USB 2.4G 接收器即插即用；BLE 扫描与自动接管；双通道自动偏好
  （USB 优先）与运行时切换，设备身份（SN）跨通道连续；
- **多设备并发**：同型号多台共用一条规则（默认每规则 4 台、全局 8 台，可配），每台设备
  独立解析管线，互不影响；
- **按键事件**：语音 / 翻译 / M 键 / AI / 截图键的按下、抬起、长按、单击；未接入键型以
  原始键值透传（`code`），**不做任何业务映射**；
- **离散事件（JSON）**：设备信息（SN / 电量 / 会议状态）、DPI 广播、会议创建/销独占调度、
  统一告警信封（配对坏态 / 设备数 / BLE 链路数预警）；
- **音频输出**：内置 SBC / ADPCM 等解码为 PCM（16 kHz / 16 bit / 单声道），或透传原始
  编码块由调用方自解码；
- **RAW 模式**：自定义硬件协议接入——自定义握手、设备匹配、SN 提取回调 + `um_write_raw`
  双向透传；
- **诊断**：进程级崩溃记录（不含业务数据）、内部日志开关、队列健康上报。

## 平台支持（跨系统 × 跨架构）

| 系统 | 架构 | USB | BLE | 交付包 |
|---|---|---|---|---|
| Windows 10 / 11 | x64 | ✅ | ✅ | 本仓库 `packages/` |
| macOS 11+ | arm64（Apple Silicon） | ✅ | ✅ | 本仓库 `packages/` |
| 统信 UOS | x86_64 / ARM64 | ✅ | ✅ | 联系商务 |
| 麒麟 Kylin | x86_64 / ARM64 | ✅ | ✅ | 联系商务 |
| 方德 FangDe | x86_64 / ARM64 | ✅ | ✅ | 联系商务 |
| 通用 Linux | x86_64 | ✅ | ✅ | 联系商务 |
| macOS（Intel） | x86_64 | ✅ | ✅ | 联系商务 |

> SDK **源码级跨平台跨架构**（CMake 工程，Windows / macOS / Linux 三系，各架构独立构建）：
> 同一套 C ABI 与 JSON 事件协议在全部平台一致，集成代码无需按平台改写；二进制按
> 「系统 × 架构」构建、不互通。本仓库当前发布 Windows x64 与 macOS ARM64 两个包，
> 其余平台组合为既有支持项，交付包联系商务获取。

## 仓库结构

```
├── README.md                  本文件
├── docs/第三方接入说明.md       完整接入文档（v2.2）
├── include/umouse_sdk.h       对外唯一头文件（纯 C ABI）
├── demo/                      C 示例（demo.c 最小接入 / demo3.c 多设备混合模式）
├── packages/                  平台交付包
│   ├── uMouseSdk_0.4.20_Windows_10-11_x64.zip
│   └── uMouseSdk_0.4.20_macOS_11_arm64.zip
├── img/                       联系二维码等图片资源
├── LICENSE                    MIT License（demo 示例代码）
├── THIRD_PARTY_LICENSES.txt   第三方组件许可与归属声明
├── COPYING.LGPLv2.1           LGPL v2.1 全文（FFmpeg）
└── LICENSE.hidapi-bsd         HIDAPI BSD-3-Clause 全文
```

## 五分钟上手

解压对应平台交付包，在仓库根目录编译运行 `demo/demo.c`：

```bat
REM Windows（MSVC 开发者命令行）
cl /I uMouseSdk\include demo\demo.c /Fe:demo.exe /link /LIBPATH:uMouseSdk uMouseSdk.lib
set PATH=%CD%\uMouseSdk;%PATH%
demo.exe
```

```bash
# macOS
cc -IuMouseSdk/include demo/demo.c -LuMouseSdk -luMouseSdk -o demo && ./demo
```

核心调用顺序：**注册回调 → `um_sdk_init()` → 业务处理 → `um_sdk_close()`**。
SDK 只负责把硬件数据忠实搬运出来（按键事件 / JSON 离散事件 / PCM 音频三线回调），
M 键做什么、音频如何识别等业务逻辑全部由调用方实现。完整步骤与各语言接入见
[docs/第三方接入说明.md](docs/第三方接入说明.md)。

## 集成方式

纯 C ABI，主流语言直接调用，无需私有运行时：

| 语言 | 方式 |
|---|---|
| C / C++ | 头文件 + 导入库直接链接（MSVC / MinGW / clang / gcc） |
| C# (.NET) | P/Invoke，无需第三方库 |
| Python | 标准库 ctypes |
| Electron / Node.js | koffi 等 FFI 库 |

## 适用场景

- 语音输入法、翻译工具、会议记录等办公效率软件的硬件入口；
- 硬件厂商基于自有协议的二次开发（RAW 模式自定义接入）；
- 行业集成商在金融、政务等信创终端（UOS / 麒麟 / 方德，x86 / ARM64）上的跨平台部署。

## 硬件适配与技术支持

如需获取信创（UOS / 麒麟 / 方德）与其他架构的交付包、硬件规格、样机、协议完整版、
企业微信二维码或技术支持资料，请联系 **安徽声云**：
[sinicloud.com](https://www.sinicloud.com/)，开放平台 [open.sinicloud.com](https://open.sinicloud.com/)。
咨询时请说明目标平台、预计数量和应用场景。

<img src="img/企业微信.png" alt="企业微信" width="200" style="max-width: 100%; height: auto;">

## 第三方组件与许可

SDK 二进制动态链接 FFmpeg（LGPL-2.1+，独立动态库形式随附，可由最终用户自行替换）
并静态编入 HIDAPI（BSD-3-Clause）；macOS 包为按实际使用的音频解码器裁剪的最小
FFmpeg 构建（无 GPL 组件，构建配置见包内 README）。署名与义务履行方式见
`THIRD_PARTY_LICENSES.txt` 与随附许可证全文。

## 许可与使用边界

仓库内 `demo/` 示例代码以 MIT License 发布。SDK 二进制库、设备私有协议和部分文档可能
包含声云或厂商专有内容，不等同于全部开源，商用范围以随包说明及双方商务协议为准。
SDK 负责把硬件数据忠实搬运出来，不替代应用侧的语音识别、业务映射等职责。请勿将
私有协议用于未获授权的硬件或产品。

## 反馈与问题

请附带 `um_sdk_init(1)` 打开的日志、目标平台与 SDK 版本号（`um_sdk_version()`），
通过仓库 Issues 反馈。

---

*关键词 / Keywords：声云智能鼠标、智能鼠标 SDK、uMouse SDK、SoniCloud、语音鼠标、AI 鼠标、mouse SDK、smart mouse SDK、BLE SDK、HID SDK、USB SDK、跨平台、跨架构、cross-platform、cross-architecture、Windows、macOS、Linux、ARM64、信创、麒麟、统信 UOS、方德 FangDe*
