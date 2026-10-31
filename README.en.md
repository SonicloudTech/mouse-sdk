# uMouse SDK — USB HID / BLE Smart Mouse Integration

[简体中文](README.md) | **English**

[![release](https://img.shields.io/badge/release-v0.4.20-blue)](https://github.com/SonicloudTech/mouse-sdk/releases/tag/v0.4.20) ![ABI](https://img.shields.io/badge/ABI-pure%20C-9cf) [![samples](https://img.shields.io/badge/demo%20samples-MIT-green)](LICENSE)

**SoniCloud Smart Mouse SDK (uMouse SDK)** connects USB HID and BLE devices through a unified C ABI, providing device discovery, connection management, key events, JSON device information, and PCM audio for hardware vendors, application developers, and system integrators. Call it from C/C++, C#, Python, or Electron / Node.js.

Requires compatible SoniCloud hardware or your own hardware adapted through RAW mode; installing the SDK does not add these capabilities to an arbitrary mouse. This repository provides **binary SDK packages, a public header, documentation, and MIT-licensed sample code**. See the [license scope](#license-scope) for SDK binaries and proprietary protocols.

[Downloads](#downloads-and-developer-resources) · [Quick start](#quick-start) · [Compatibility & FAQ](#faq) · [Hardware & support](#hardware-access)

## Downloads and Developer Resources

| Resource | Link |
|---|---|
| Windows 10 / 11 x64 · SDK 0.4.20 | [Download Windows ZIP](https://github.com/SonicloudTech/mouse-sdk/releases/download/v0.4.20/uMouseSdk_0.4.20_Windows_10-11_x64.zip) |
| macOS 11+ ARM64 · SDK 0.4.20 | [Download Apple Silicon ZIP](https://github.com/SonicloudTech/mouse-sdk/releases/download/v0.4.20/uMouseSdk_0.4.20_macOS_11_arm64.zip) |
| Release notes and all releases | [GitHub Releases](https://github.com/SonicloudTech/mouse-sdk/releases) |
| Full integration documentation v2.2 | [English guide](docs/Integration-Guide.en.md) · [中文指南](docs/第三方接入说明.md) |
| C samples and public API | [Choose a sample](#examples) · [umouse_sdk.h](include/umouse_sdk.h) |
| Linux / UOS / Kylin / FangDe packages, compatible hardware, and evaluation units | [Access and technical support](#hardware-access) |

## Quick Start

1. Get the sample source code:

   ```sh
   git clone https://github.com/SonicloudTech/mouse-sdk.git
   cd mouse-sdk
   ```

2. Download the ZIP matching your OS and architecture above. Extract it into the repository root and confirm that `uMouseSdk/include/umouse_sdk.h` and the SDK libraries are present. The same two platform packages are also available in [packages/](packages/).
3. Connect compatible hardware, then build and run the minimal sample from the repository root. On Windows, use an **x64 MSVC developer command prompt**; on macOS, install Xcode Command Line Tools.

**Windows x64:**

```bat
cl /I uMouseSdk\include demo\demo.c /Fe:umouse-demo.exe /link /LIBPATH:uMouseSdk uMouseSdk.lib
set "PATH=%CD%\uMouseSdk;%PATH%"
umouse-demo.exe
```

**macOS ARM64:**

```sh
cc -IuMouseSdk/include demo/demo.c -LuMouseSdk -luMouseSdk -Wl,-rpath,@executable_path/uMouseSdk -o umouse-demo
./umouse-demo
```

Keep the packaged runtime libraries alongside the main SDK library. See the package README for Windows DLL and VC runtime loading details. On macOS, allow Bluetooth access when first using BLE; the package includes its FFmpeg libraries, so no separate installation is needed.

The sample prints the SDK version, followed by `[connected]` when a device comes online, then device events and audio lengths. Received PCM is saved to `out.pcm`. Press Enter to exit. **A version string confirms library loading; use device connection events and callback data to confirm the hardware connection.**

Core sequence: **register callbacks → `um_sdk_init()` → run your application → `um_sdk_close()`**. The SDK supplies key events, device events, and audio; speech recognition and key-to-action mapping belong to your application. See the [full integration guide](docs/Integration-Guide.en.md) for details and troubleshooting.

<a id="examples"></a>

## Choose a Sample

| Use case | Sample |
|---|---|
| SoniCloud device basics: keys, device information, and PCM audio | [demo.c](demo/demo.c) — minimal FULL-mode integration |
| Raw frames from a SoniCloud device, parsed by your application | [demo2.c](demo/demo2.c) — RAW mode, handshake, and channel switching |
| SoniCloud devices and custom devices connected together | [demo3.c](demo/demo3.c) — multiple rules and devices, FULL / RAW coexistence |
| External hardware with custom identifiers, handshake, and parsing | [demo4.c](demo/demo4.c) — complete example replacing the default rule |

`demo4.c` currently uses real SoniCloud identifiers and protocol messages to demonstrate the integration flow. For your own hardware, replace its `HW_*` configuration, handshake, and parsing logic. A custom USB hardware data channel must occupy the selected interface's first HID top-level collection or a dedicated interface, and support Output Report or Feature Report writes. See the [sample guide (Chinese)](demo/README.md) for the complete prerequisites and sample selection; arbitrary USB / BLE hardware still requires adaptation.

## Capabilities

- **Device discovery and connection:** plug-and-play USB 2.4G receivers, BLE scanning and automatic takeover, USB-preferred channel selection, and runtime switching with stable device identity (SN) across channels.
- **Multiple devices:** units of the same model can share a rule; configurable defaults are 4 devices per rule and 8 globally, with independent parsing per device.
- **Key events:** speech / translation / M / AI / capture keys with down, up, hold, and click actions. Unsupported key types pass through as raw `code` values, without application-specific mapping.
- **JSON device events:** SN, battery, meeting state, DPI broadcasts, meeting lifecycle and exclusive scheduling, plus pairing, device-count, and BLE-link-count warnings.
- **Audio:** built-in SBC / ADPCM and other decoding to PCM (16 kHz / 16-bit / mono), or raw encoded blocks for application-side decoding.
- **RAW mode:** custom handshake, device matching, SN extraction, and protocol parsing, with bidirectional transfer through `um_write_raw`.
- **Diagnostics:** process crash records excluding application data, internal logging controls, and queue-health reporting.

## Platforms and Architectures

| OS | Architecture | USB | BLE | Distribution |
|---|---|---|---|---|
| Windows 10 / 11 | x64 | ✅ | ✅ | [Public download](https://github.com/SonicloudTech/mouse-sdk/releases/download/v0.4.20/uMouseSdk_0.4.20_Windows_10-11_x64.zip) |
| macOS 11+ | ARM64 (Apple Silicon) | ✅ | ✅ | [Public download](https://github.com/SonicloudTech/mouse-sdk/releases/download/v0.4.20/uMouseSdk_0.4.20_macOS_11_arm64.zip) |
| UOS (UnionTech) | x86_64 / ARM64 | ✅ | ✅ | [Contact us](#hardware-access) |
| Kylin | x86_64 / ARM64 | ✅ | ✅ | [Contact us](#hardware-access) |
| FangDe | x86_64 / ARM64 | ✅ | ✅ | [Contact us](#hardware-access) |
| Generic Linux | x86_64 | ✅ | ✅ | [Contact us](#hardware-access) |
| macOS (Intel) | x86_64 | ✅ | ✅ | [Contact us](#hardware-access) |

The internal SDK source uses CMake to build for different operating systems and architectures, with a shared public C ABI and JSON event protocol. **This repository distributes packages and integration resources, not the complete SDK source.** Windows x64 and macOS ARM64 packages are publicly downloadable; contact us for the other combinations above. Binaries are built separately for each OS and architecture and are not interchangeable; Linux distributions also require matching packages.

## Language Integration

| Language | Integration method |
|---|---|
| C / C++ | Header and import library, MSVC / MinGW / clang / gcc |
| C# / .NET | P/Invoke |
| Python | Standard-library `ctypes` |
| Electron / Node.js | FFI libraries such as `koffi` |

Language snippets, callback lifetime rules, and API details are available in the [English integration guide](docs/Integration-Guide.en.md) and [Chinese integration guide](docs/第三方接入说明.md). The standalone compilable samples in this repository are the four C programs above.

## Use Cases and Related Project

- Hardware input for voice typing, translation tools, and meeting-note applications.
- Hardware vendors integrating their own device protocols through RAW mode.
- System integrators deploying device connectivity on x86_64 / ARM64 UOS, Kylin, FangDe, and other Chinese domestic IT platforms.

For AI agents operating a physical mouse, see [mouse-mcp (uMouse MCP server)](https://github.com/SonicloudTech/mouse-mcp). It exposes mouse actions and voice-input integration through MCP and compatible uMouse USB hardware, **currently on Windows x64**. This SDK serves application and hardware integration; consult the MCP repository for its own platform and hardware requirements.

## Repository Layout

```text
├── README.md                    Chinese README
├── README.en.md                 English README
├── docs/第三方接入说明.md        Chinese integration guide v2.2
├── docs/Integration-Guide.en.md  English integration guide v2.2
├── include/umouse_sdk.h          Public C ABI header
├── demo/                        demo.c / demo2.c / demo3.c / demo4.c
├── packages/                    SDK 0.4.20: Windows x64 / macOS ARM64 ZIPs
├── img/                         Contact QR code
├── LICENSE                      MIT sample-code license
├── THIRD_PARTY_LICENSES.txt      Third-party notices
├── COPYING.LGPLv2.1             FFmpeg license text
└── LICENSE.hidapi-bsd            HIDAPI license text
```

<a id="hardware-access"></a>

## Hardware, Other Platform Packages, and Support

Contact **SoniCloud (安徽声云智能科技有限公司)** for compatible hardware specifications, evaluation units, the full protocol, Linux / Chinese domestic OS and other architecture packages, commercial licensing, or technical support. Include your target OS and architecture, device model, expected volume, and use case.

- Business and technical contact: [xu_heng_yu@sinicloud.com](mailto:xu_heng_yu@sinicloud.com)
- Website: [sinicloud.com](https://sinicloud.com/)
- Speech open platform: [open.sinicloud.com](https://open.sinicloud.com/)

<img src="img/微信.png" alt="SoniCloud technical and business contact WeChat QR code" width="200">

<a id="license-scope"></a>

## License Scope and Third-Party Components

Sample code under `demo/` is released under the [MIT License](LICENSE). SDK binaries, proprietary device protocols, and some documents may contain proprietary content belonging to SoniCloud or its partners; the project is not open source as a whole. Commercial use is governed by the accompanying notices and the agreement between the parties. The SDK does not replace application-side speech recognition or business mapping. Do not use proprietary protocols for unauthorized hardware or products.

SDK binaries dynamically link FFmpeg (LGPL-2.1+, shipped as separate libraries that end users can replace) and statically embed HIDAPI (BSD-3-Clause). The macOS package uses a minimal FFmpeg build containing only the required audio decoders and no GPL components; its build configuration is in the package README. See the [third-party notices](THIRD_PARTY_LICENSES.txt), [LGPL text](COPYING.LGPLv2.1), and [HIDAPI license](LICENSE.hidapi-bsd) for attribution and use requirements.

<a id="faq"></a>

## USB HID / BLE SDK: frequently asked questions

### Is this a universal driver for any USB or Bluetooth mouse?

No. FULL mode handles compatible SoniCloud devices. Your own hardware requires RAW-mode adaptation of device matching, handshake, and frame parsing. Start with the [sample selection guide](demo/README.md) and `demo4.c`; having a USB or BLE connection alone is insufficient.

### Can I use the SDK from Python, Electron, or C#?

Yes, through the public C ABI: Python uses `ctypes`, Electron / Node.js can use an FFI library such as `koffi`, and C# uses P/Invoke. Match the application's process architecture to the native package. Keep callback objects alive, copy callback buffers before returning if retaining them, and follow the lifecycle rules in the [integration guide](docs/Integration-Guide.en.md). The standalone samples in this repository are C programs, not packaged applications for every language.

### Why does the library fail to load even when uMouseSdk.dll exists?

Check the process architecture, library search path, and all runtime dependencies in the downloaded package. Keep its FFmpeg and VC runtime DLLs with the SDK and follow the package README's loading instructions. Do not copy only the main DLL. On macOS, keep the bundled dylibs together and allow Bluetooth access when using BLE.

### Where are the Linux and ARM64 downloads?

The current public [v0.4.20 release](https://github.com/SonicloudTech/mouse-sdk/releases/tag/v0.4.20) contains Windows x64 and macOS ARM64 packages. Linux, UOS, Kylin, FangDe, and other combinations listed in the [platform matrix](#platforms-and-architectures) are available through [technical support](#hardware-access). Request the exact OS and architecture; binaries are not interchangeable.

### Does the SDK include speech recognition or an MCP server?

The SDK supplies device events and audio; your application implements speech recognition and business actions. For AI-agent hardware mouse control, use the separate [mouse-mcp server](https://github.com/SonicloudTech/mouse-mcp), whose published packages target Windows x64 and require compatible hardware. SDK binary licensing is separate from the [MIT sample-code license](#license-scope).

## Reporting Issues

Open a [GitHub issue](https://github.com/SonicloudTech/mouse-sdk/issues) with your target OS and architecture, device model, SDK version (`um_sdk_version()`), reproduction steps, and relevant logs enabled by `um_sdk_init(1)`.
