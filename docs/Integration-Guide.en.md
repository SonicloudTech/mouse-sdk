# SoniCloud Smart Mouse SDK · Integration Guide (v0.4.20)

Languages: [简体中文](第三方接入说明.md) · **English**

| Item | Content |
|---|---|
| Product | SoniCloud Smart Mouse SDK (dynamic library `uMouseSdk`) |
| SDK version | **0.4.20** (as returned by `um_sdk_version()`) |
| Document version | v2.2 (aligned with 0.4.20: delivery as a release repository + single-library platform packages, platform matrix updated to actual delivery channels, macOS dependency/permission notes corrected, FAQ multi-device wording and threading model synced with reality; English translation of the Chinese v2.2) |
| Audience | Third-party integrators |
| Form factor | One dynamic library + one header file (pure C ABI) |
| Device identity | `device_id` = device SN (multi-device model; single-device compatibility path in 5.6) |

---

## 1. Product Overview

This SDK communicates with SoniCloud Smart Mouse hardware. It encapsulates device discovery, protocol parsing and audio decoding over both **USB (HID)** and **BLE** (Bluetooth Low Energy), and delivers three kinds of data:

1. **Raw key events** — which key + which action (down/up/hold/click), with **no business mapping applied**;
2. **Discrete events** — device info (serial number / battery), DPI changes, meeting state, etc., reported as JSON strings;
3. **Audio data** — two output modes (see "7. Audio Data"): decoded **PCM** (16 kHz / 16-bit / mono) by default, or passthrough of **raw encoded blocks** (RAW).

> **Design principle**: the SDK only faithfully carries hardware data out — **all business logic is implemented by the caller**. Whether an M-key press triggers a screenshot, speech-to-text, or Enter is decided by your program based on the received `keyEvent`. This keeps the public interface stable over time; hardware upgrades only add fields to the JSON.

### Work Modes

Two work modes are provided (`um_set_work_mode`):

| Mode | Macro | Description |
|---|---|---|
| FULL | `UM_MODE_FULL` (default) | Full protocol parsing: keys/audio/SN/DPI/meeting parsed by the SDK and reported via `um_on_message` / `um_on_audio` |
| RAW | `UM_MODE_RAW` | Identification-only: the SDK performs no protocol parsing and passes raw frames through `um_on_raw_frame`; the host parses them |

**How to choose**:

- Same hardware protocol → `FULL`;
- Different audio chip → `RAW` + `um_set_audio_output_mode(UM_AUDIO_RAW)`;
- Different hardware protocol → `RAW`, handle frames yourself in `um_on_raw_frame`, send commands with `um_write_raw`.

### Platform Support Matrix

| OS | Architecture | Connections | Package |
|---|---|---|---|
| Windows 10 / 11 | x64 | USB ✅ / BLE ✅ | This repository `packages/` |
| macOS 11+ | arm64 (Apple Silicon) | USB ✅ / BLE ✅ | This repository `packages/` |
| UOS (UnionTech) | x86 (amd64) / ARM64 (aarch64) | USB ✅ / BLE ✅ | Contact sales |
| Kylin | x86 (amd64) / ARM64 (aarch64) | USB ✅ / BLE ✅ | Contact sales |
| FangDe | x86 (amd64) / ARM64 (aarch64) | USB ✅ / BLE ✅ | Contact sales |
| Generic Linux | x86 (amd64) | USB ✅ / BLE ✅ | Contact sales |
| macOS (Intel) | x86_64 | USB ✅ / BLE ✅ | Contact sales |

> **Important**: the dynamic libraries are **built per platform** and are not interchangeable — pick the package matching your deployment target. Although UOS / Kylin / FangDe are all Linux/glibc, system library versions differ; use the package built for each system.

---

## 2. Package Structure

The SDK ships as a **single library + header + dependency closure**. This repository's `packages/` provides the Windows and macOS packages (zip); unzipping either yields a self-contained `uMouseSdk/` directory. License files and the C sample sources (`demo/`) live at the repository root.

```
<release repository>/
├── include/umouse_sdk.h       # the single public header (identical to the one in packages)
├── demo/                      # C sample sources (demo.c / demo2.c / demo3.c)
├── packages/                  # platform packages (zip)
├── THIRD_PARTY_LICENSES.txt   # third-party notices
├── COPYING.LGPLv2.1           # LGPL v2.1 full text (FFmpeg)
└── LICENSE.hidapi-bsd         # HIDAPI BSD-3-Clause full text
```

### Windows package (uMouseSdk_0.4.20_Windows_10-11_x64.zip)

```
uMouseSdk/
├── uMouseSdk.dll             # main library
├── uMouseSdk.lib             # link-time import library (MSVC; ignored with LoadLibrary)
├── avcodec-58.dll            # FFmpeg runtime dependency (LGPL, dynamically linked)
├── avutil-56.dll
├── msvcp140.dll              # VC++ runtime copies (loading notes in the package README)
├── vcruntime140.dll
├── vcruntime140_1.dll
├── include/umouse_sdk.h
└── README.md                 # loading (PATH / LOAD_WITH_ALTERED_SEARCH_PATH / CRT preloading)
```

### macOS package (uMouseSdk_0.4.20_macOS_11_arm64.zip)

```
uMouseSdk/
├── libuMouseSdk.dylib        # main library (also .0 / .0.4.20 versioned link names, all equivalent)
├── libavcodec.62.dylib       # trimmed FFmpeg runtime dependency (LGPL, dynamically linked;
├── libavutil.60.dylib        #   versioned files included, dependencies rewritten to
│                             #   @loader_path same-directory lookup — no brew required)
├── include/umouse_sdk.h
└── README.md
```

### Xinchuang platforms / Generic Linux / macOS Intel (x86_64)

Packages for these platforms are outside this repository's releases — contact sales. Their layout is `include/ + lib/libuMouseSdk.so`; integration is identical to the steps below.

> The `include/umouse_sdk.h` file is identical on all platforms. You only ever `#include` this one header.

---

## 3. Quick Start

The fastest verification path: build and run the bundled sample source (`demo/demo.c`, a ~60-line minimal example) to confirm hardware connection and data flow.

### 3.1 Windows

Unzip `packages/uMouseSdk_0.4.20_Windows_10-11_x64.zip`, then at the repository root:

```bat
REM MSVC developer command prompt
cl /I uMouseSdk\include demo\demo.c /Fe:demo.exe /link /LIBPATH:uMouseSdk uMouseSdk.lib

REM Add the package directory to the process PATH before running
REM (the DLL directory is not on the default search path)
set PATH=%CD%\uMouseSdk;%PATH%
demo.exe
```

(MinGW: `gcc -IuMouseSdk/include demo/demo.c -LuMouseSdk -luMouseSdk -o demo.exe`.
Full loading notes are in the package README: PATH prepend / `LoadLibraryExW` + `LOAD_WITH_ALTERED_SEARCH_PATH` / VC++ runtime preload order.)

### 3.2 macOS

Unzip `packages/uMouseSdk_0.4.20_macOS_11_arm64.zip`, then at the repository root:

```bash
cc -IuMouseSdk/include demo/demo.c -LuMouseSdk -luMouseSdk -o demo
./demo
```

The first run requests Bluetooth permission; USB (2.4G dongle) works plug-and-play.

### 3.3 Linux / Xinchuang systems

Xinchuang / generic Linux packages come through sales (`include/ + lib/libuMouseSdk.so`); build as above (link `-luMouseSdk`). USB requires the device access permission to be installed first (see "8. Platform Notes").

Expected output (illustrative):

```
声云智能鼠标 SDK demo, version 0.4.20
[连接] 000AEB20... via USB
[事件] 000AEB20... {"type":"deviceInfo","sn":"000AEB20...","battery":50,"deviceType":"usb","status":"normal"}
[audio len=] 320
运行中... 按回车结束。
```

---

## 4. Integrating Into Your Project

The SDK is a **pure C ABI** callable from virtually every mainstream language. Core sequence:

> **register callbacks → `um_sdk_init()` → run your business → `um_sdk_close()`**

> ⚠️ Callbacks **must be registered before `um_sdk_init()`**, otherwise early events (such as device connection) may be missed.

### 4.1 C / C++

**Compile/link:**

- Header: add the package's `include/` to your include path;
- Windows (MSVC): link `uMouseSdk.lib` from the package; at runtime place all DLLs from the package next to your executable (or add the package directory to `PATH`);
- Linux: link `-luMouseSdk` (`-L<package>/lib`), set `LD_LIBRARY_PATH` or use the rpath `$ORIGIN`;
- macOS: link `-luMouseSdk` (`-L<package>/uMouseSdk`), use `@loader_path` as rpath.

**Minimal example (FULL mode):**

```c
#include "umouse_sdk.h"
#include <stdio.h>

static void on_connected(const char* id, int mode) {
    printf("[connected] %s via %s\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}
static void on_disconnected(const char* id, int mode) {
    printf("[disconnected] %s via %s\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}
static void on_message(const char* id, const char* json) {
    printf("[event] %s %s\n", id, json);
}
static void on_audio(const char* id, const unsigned char* pcm, int len) {
    /* pcm is only valid during the callback; memcpy immediately if you keep it */
}

int main(void) {
    um_register_connected(on_connected);
    um_register_disconnected(on_disconnected);
    um_register_message(on_message);
    um_register_audio(on_audio);

    um_sdk_init(1);   /* debug=1 enables internal logging */

    getchar();        /* simulate your business loop */

    um_sdk_close();
    return 0;
}
```

**RAW mode example (when the hardware protocol differs from the default):**

```c
#include "umouse_sdk.h"

/* RAW mode + USB channel only: build the handshake probe */
static int build_probe(uint8_t* out, int max_len) {
    /* return the number of bytes written; <= 0 means use the default probe */
    return 0;
}
/* RAW mode + USB channel only: validate a response frame */
static int validate_resp(const uint8_t* buf, int len) {
    /* return 1 = valid response (device online), 0 = not */
    return 1;
}
/* RAW mode + BLE channel only: decide whether a discovered device is a target */
static int match_device(const char* name, const char* address) {
    /* return 1 = target (take over), 0 = skip */
    return 1;
}
static void on_raw_frame(const char* id, const uint8_t* data, int len, int conn) {
    /* raw frames reported by the device — parse them yourself */
}

int main(void) {
    um_set_work_mode(UM_MODE_RAW);           /* set before init */
    um_set_audio_output_mode(UM_AUDIO_RAW);  /* if you want raw encoded blocks */

    um_register_raw_frame(on_raw_frame);
    um_register_usb_build_probe(build_probe);
    um_register_usb_validate_resp(validate_resp);
    um_register_ble_match_device(match_device);

    um_sdk_init(1);
    getchar();
    um_sdk_close();
}
```

**CMake example:**

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app C)

add_executable(my_app main.c)

set(UMSDK_DIR "<your path>/uMouseSdk")   # unzipped Windows package directory

target_include_directories(my_app PRIVATE "${UMSDK_DIR}/include")
target_link_directories(my_app PRIVATE "${UMSDK_DIR}")
target_link_libraries(my_app PRIVATE uMouseSdk)
```

### 4.2 C# (.NET)

Call via **P/Invoke** — no third-party libraries needed.

```csharp
using System;
using System.Runtime.InteropServices;

internal static class UMouseSdk
{
    private const string DLL = "uMouseSdk";   // Linux: "libuMouseSdk.so", macOS: "libuMouseSdk.dylib"

    public const int UM_CONN_USB = 0;
    public const int UM_CONN_BLE = 1;
    public const int UM_MODE_FULL = 0;
    public const int UM_MODE_RAW  = 1;
    public const int UM_AUDIO_PCM = 0;
    public const int UM_AUDIO_RAW = 1;
    public const int UM_CHAN_AUTO = 0;
    public const int UM_CHAN_USB  = 1;
    public const int UM_CHAN_BLE  = 2;

    public delegate void OnConnected(IntPtr deviceId, int connMode);
    public delegate void OnDisconnected(IntPtr deviceId, int connMode);
    public delegate void OnMessage(IntPtr deviceId, IntPtr json);
    public delegate void OnAudio(IntPtr deviceId, IntPtr pcm, int len);
    public delegate void OnRawFrame(IntPtr deviceId, IntPtr data, int len, int conn);

    [DllImport(DLL)] public static extern void um_sdk_init(int debug);
    [DllImport(DLL)] public static extern void um_sdk_close();
    [DllImport(DLL)] public static extern IntPtr um_sdk_version();

    [DllImport(DLL)] public static extern int um_set_work_mode(int mode);
    [DllImport(DLL)] public static extern int um_set_audio_output_mode(int mode);
    [DllImport(DLL)] public static extern int um_set_channel_pref(int pref);

    [DllImport(DLL)] public static extern void um_register_connected(OnConnected cb);
    [DllImport(DLL)] public static extern void um_register_disconnected(OnDisconnected cb);
    [DllImport(DLL)] public static extern void um_register_message(OnMessage cb);
    [DllImport(DLL)] public static extern void um_register_audio(OnAudio cb);
    [DllImport(DLL)] public static extern void um_register_raw_frame(OnRawFrame cb);

    [DllImport(DLL)] public static extern int um_get_device_count();
    [DllImport(DLL)] public static extern IntPtr um_get_device_id(int index);
    [DllImport(DLL)] public static extern int um_get_connection_mode(IntPtr deviceId);

    [DllImport(DLL)] public static extern int um_write_raw(IntPtr deviceId, byte[] data, int len);
    [DllImport(DLL)] public static extern int um_set_dpi(IntPtr deviceId, int level);
    [DllImport(DLL)] public static extern int um_meeting_create(IntPtr deviceId);
    [DllImport(DLL)] public static extern int um_meeting_destroy(IntPtr deviceId);
    [DllImport(DLL)] public static extern int um_meeting_pause(IntPtr deviceId);
    [DllImport(DLL)] public static extern int um_meeting_resume(IntPtr deviceId);

    public static string PtrToStringUtf8(IntPtr p) =>
        p == IntPtr.Zero ? null : Marshal.PtrToStringUTF8(p);
}
```

> **C# reminder**: delegate instances used for P/Invoke **must be kept alive** (store them as fields or static members); otherwise the CLR GC may collect them and the SDK callback will hit freed memory and crash.

### 4.3 Python

Using the standard library `ctypes`:

```python
import ctypes
from ctypes import c_int, c_char_p, c_void_p, CFUNCTYPE

lib = ctypes.CDLL("./libuMouseSdk.so")   # change the suffix per platform

ON_CONNECTED = CFUNCTYPE(None, c_char_p, c_int)
ON_MESSAGE   = CFUNCTYPE(None, c_char_p, c_char_p)
ON_AUDIO     = CFUNCTYPE(None, c_char_p, c_void_p, c_int)

@ON_CONNECTED
def on_connected(device_id, mode):
    print(f"[connected] {device_id.decode()} via {'BLE' if mode else 'USB'}")

@ON_MESSAGE
def on_message(device_id, json):
    print(f"[event] {device_id.decode()} {json.decode()}")

@ON_AUDIO
def on_audio(device_id, pcm, length):
    buf = ctypes.string_at(pcm, length)   # copy out immediately

lib.um_register_connected(on_connected)
lib.um_register_message(on_message)
lib.um_register_audio(on_audio)

lib.um_sdk_init(1)
print("SDK version:", lib.um_sdk_version().decode())

input("Press Enter to exit...\n")
lib.um_sdk_close()
```

> **Python reminder**: callback objects decorated with `@CFUNCTYPE` must be **kept referenced** (module-level variables are fine) — never pass temporaries to register.

### 4.4 Electron / Node.js

[koffi](https://github.com/Koromix/koffi) is recommended:

```js
const koffi = require('koffi');
const lib = koffi.load('./libuMouseSdk.so');   // change the suffix per platform

const um_sdk_init    = lib.func('void um_sdk_init(int debug)');
const um_sdk_close   = lib.func('void um_sdk_close()');
const um_sdk_version = lib.func('const char* um_sdk_version()');
const um_register_msg = lib.func('void um_register_message(void *cb)');

const OnMessage = koffi.proto('void OnMessage(const char* id, const char* json)');
const cb = koffi.register((id, json) => {
    console.log('[event]', koffi.decode(id), JSON.parse(koffi.decode(json)));
}, koffi.pointer(OnMessage));

um_register_msg(cb);
um_sdk_init(1);
console.log('SDK version:', koffi.decode(um_sdk_version()));
```

---

## 5. API Reference

All symbols are declared in `umouse_sdk.h`, all `extern "C"`, all prefixed `um_`.

### 5.1 Lifecycle

```c
void        um_sdk_init(int debug);
void        um_sdk_close(void);
const char* um_sdk_version(void);
void        um_enable_crash_report(int enable);
```

| Function | Description |
|---|---|
| `um_sdk_init` | Initializes and starts the SDK (internally starts device monitoring plus per-device parsing/audio-decode threads; events are dispatched on a single thread). `debug != 0` enables internal logging. **Register callbacks before calling this.** |
| `um_sdk_close` | Stops and releases SDK resources (idempotent, may be called repeatedly). Call once before exit. |
| `um_sdk_version` | Returns the version string (e.g. `"0.4.20"`); static string, valid for the whole process lifetime. |
| `um_enable_crash_report` | Process-level crash recorder switch, **on by default**. When enabled, an unhandled crash writes `umouse_crash-<timestamp>.log` into the **current working directory** (exception code, fault address, faulting module, raw stack, SDK version; the latest 10 files are kept — older ones are cleaned at the next `um_sdk_init`). Technical diagnostics only — no token/SN business data. `enable=0` disables; callable before `um_sdk_init` or at any time, effective immediately. Disable if the host has its own crash capture (Breakpad/crashpad) to avoid double-writing. |

### 5.2 Configuration

```c
/* Audio output mode */
#define UM_AUDIO_PCM 0   /* built-in decoding to PCM: 16 kHz/16-bit/mono (default); supports ADPCM/SBC */
#define UM_AUDIO_RAW 1   /* raw encoded accumulated blocks: undecoded (256 B per block), decode yourself */
int um_set_audio_output_mode(int mode);

/* Work mode */
#define UM_MODE_FULL 0   /* full protocol parsing (default for the default rule) */
#define UM_MODE_RAW  1   /* identification only: raw-frame passthrough, no parsing (default for non-default rules) */
int um_set_work_mode(int mode);
int um_set_work_mode_for(const char* rule_id, int mode);   /* per rule (multi-device, see 5.7) */

/* Multiple units of the same model: max simultaneous devices per rule
 * (default 4, range 1~8). Since v0.4.0 every online device gets an
 * independent parsing pipeline (keys/audio/SN/DPI/meeting all parsed);
 * beyond the limit, newly discovered devices simply stay offline. */
int um_set_device_limit(const char* rule_id, int max_devices);

/* USB (2.4G dongle) identification */
struct um_usb_config {
    unsigned short vid;         /* vendor ID; 0 = default 0xABC9 */
    unsigned short pid;         /* product ID; 0 = default 0xCA89 */
    int iface;                  /* data interface number; <0 = any interface */
    unsigned char report_id;    /* HID report ID; 0 = default 0x0A */
};
int um_set_usb_config(const char* rule_id, const struct um_usb_config* cfg);

/* Bluetooth identification */
struct um_ble_config {
    const char* name;           /* exact device-name match; NULL/"" = default "uMouse" */
    const char* mac_prefix;     /* MAC prefix match (case-insensitive); NULL/"" = default "C0:88" */
    const char* service;        /* GATT service UUID; NULL/"" = default */
    const char* characteristic; /* GATT notify characteristic UUID; NULL/"" = default */
};
int um_set_ble_config(const char* rule_id, const struct um_ble_config* cfg);

/* Channel preference (changeable at runtime) */
#define UM_CHAN_AUTO 0   /* USB preferred, silently fall back to BLE after timeout (default) */
#define UM_CHAN_USB  1   /* probe USB only, never switch to BLE */
#define UM_CHAN_BLE  2   /* probe BLE only, skip the USB dongle */
int um_set_channel_pref(int pref);                            /* applies to all online devices */
int um_set_channel_pref_for(const char* device_id, int pref); /* applies to one device */
```

**Timing constraints**:

| Function | When | Notes |
|---|---|---|
| `um_set_work_mode` / `um_set_work_mode_for` | only before `um_sdk_init` | locked at init, later calls return 0; only one FULL rule globally |
| `um_set_device_limit` | only before `um_sdk_init` | same-model cap (default 4, 1~8; per-device pipelines) |
| `um_set_usb_config` | only before `um_sdk_init` | after init returns 0; 0 fields keep defaults; rule_id registers a rule (see 5.7) |
| `um_set_ble_config` | only before `um_sdk_init` | after init returns 0; strings are copied immediately, caller may free them |
| `um_set_audio_output_mode` | any time, even after init | affects only what `um_on_audio` delivers |
| `um_set_channel_pref` / `um_set_channel_pref_for` | only after `um_sdk_init` | runtime action; switching drops the current channel and re-probes |

> All configuration functions returning `int`: **`1` = accepted, `0` = rejected / invalid parameter**.

### 5.3 Callback Types

```c
/* Device connected. conn_mode: UM_CONN_USB / UM_CONN_BLE */
typedef void (*um_on_connected)(const char* device_id, int conn_mode);

/* Device disconnected */
typedef void (*um_on_disconnected)(const char* device_id, int conn_mode);

/* Discrete event; json is a UTF-8 string (see "6. Event Protocol") */
typedef void (*um_on_message)(const char* device_id, const char* json);

/* PCM audio: 16 kHz/16-bit/mono little-endian. pcm valid only during the callback */
typedef void (*um_on_audio)(const char* device_id, const uint8_t* pcm, int len);

/* Raw frame (full passthrough for RAW rules; symmetric per device for FULL rules since v0.4.0).
 * data valid only during the callback */
typedef void (*um_on_raw_frame)(const char* device_id, const uint8_t* data, int len, int conn);

/* USB handshake probe builder (RAW mode + USB channel only)
 * Fill the handshake bytes into out and return the length (<= max_len); <= 0 uses the default probe */
typedef int (*um_usb_build_probe)(uint8_t* out, int max_len);

/* USB handshake response validator (RAW mode + USB channel only)
 * Return 1 = valid response (device online), 0 = not */
typedef int (*um_usb_validate_resp)(const uint8_t* buf, int len);

/* BLE target-device matcher (RAW mode + BLE channel only)
 * Return 1 = target device (take over), 0 = skip */
typedef int (*um_ble_match_device)(const char* name, const char* address);
```

### 5.4 Callback Registration

```c
void um_register_connected(um_on_connected cb);
void um_register_disconnected(um_on_disconnected cb);
void um_register_message(um_on_message cb);
void um_register_audio(um_on_audio cb);
void um_register_raw_frame(um_on_raw_frame cb);                 /* effective in RAW mode */
void um_register_usb_build_probe(um_usb_build_probe cb);         /* RAW-mode USB handshake */
void um_register_usb_validate_resp(um_usb_validate_resp cb);     /* RAW-mode USB validation */
void um_register_ble_match_device(um_ble_match_device cb);       /* RAW-mode BLE identification */
```

- Pass `NULL` or simply don't register callbacks you don't need — the SDK never crashes for missing ones;
- Pointers such as `device_id` / `json` / `pcm` / `data` are **valid only during the callback** — copy immediately if you keep them;
- Register before `um_sdk_init` to avoid missing early events.

### 5.5 Device Queries (synchronous)

```c
int         um_get_device_count(void);          /* currently connected devices (multi-device model) */
const char* um_get_device_id(int index);        /* device id at index; NULL when out of range */
int         um_get_connection_mode(const char* id); /* 0=USB 1=BLE; -1 when not connected */
```

> Multi-device model since v0.3.0: `um_get_device_count()` returns **the number of fully identified online devices**; `device_id` is the device SN (or a temporary transport identity after identification timeout, see 5.7); `NULL` means "the only online device" — with multiple devices online, queries return -1 and commands return 0.

### 5.6 Commands / Requests

All command functions return `int`: **`1` = command sent, `0` = failed** (e.g. not connected). Asynchronous results arrive via the `um_on_message` callback.

```c
/* Write raw bytes to the device's active channel (works in both work modes).
 * USB-HID usually requires the report_id as the first byte (default 0x0A), assembled by the caller.
 * BLE must not carry the report_id prefix. */
int um_write_raw(const char* device_id, const uint8_t* data, int len);

int um_set_dpi(const char* device_id, int level);   /* set DPI level (see note below) */

int um_meeting_create(const char* device_id);       /* create a meeting */
int um_meeting_destroy(const char* device_id);      /* end the meeting */
int um_meeting_pause(const char* device_id);        /* pause the meeting */
int um_meeting_resume(const char* device_id);       /* resume the meeting */
```

> `device_id` is the target device's SN; `NULL` works only while **exactly one** device is online (the single-device compatibility path) — with multiple devices online it returns 0. Session operations (meeting/DPI/raw writes) must address a specific device in multi-device setups.
>
> **Meeting exclusivity**: globally, only one device may be in a meeting at a time. While a meeting is active, meeting commands for other devices are **intercepted in the SDK layer** (return 0) and the intercepted device receives `{"type":"meetingBusy","owner":"<owner SN>"}`. Ownership is released when the meeting ends (`meetingDestroyed`) or the owning device disconnects. Since v0.4.0 every device has its own pipeline; this is the only form of meeting conflict.
>
> **Audio source: first come, first served** (default policy): when several devices capture simultaneously, the first device to produce audio holds the audio source until **its capture segment ends** (speech key release / device offline); other devices are muted in the SDK layer during that time — their keyEvents are still reported, and the host can show an "occupied" hint. The next capture segment starts first-come-first-served again. Use `um_set_audio_source` to pin a device explicitly (see 5.7).
>
> **DPI note**: the `level` passed to `um_set_dpi` is a **firmware-specific level byte** (protocol `03 22 <level>`) — not a 1..N index and not a DPI value. Mainstream firmware maps {800→21, 1200→32, 1600→42, 2400→63, 4000→4, 6000→5}; other firmware families encode differently (e.g. 0-based index). Build the level↔byte mapping per channel/firmware yourself — the SDK only passes the byte through. Firmware does **not** acknowledge software DPI setting: a return of 1 only means the bytes were written to the transport; verify by cursor speed. `dpiChanged` events are unrelated to software setting (see 6.3).
>
> For `um_write_raw`, first check the active channel with `um_get_connection_mode`: USB frames carry a report_id prefix, BLE frames don't (per the target device's protocol).

### 5.7 Multi-Device Access (since v0.3.0)

**Device model**:

- **Device identity = SN**. USB and BLE are two transports of the same device; only one is active at a time. Switching transports preserves the identity (`device_id` unchanged, `conn_mode` changes).
- **"Device type" is described by a rule**: the first parameter of `um_set_usb_config` / `um_set_ble_config` / `um_set_work_mode_for` registers a rule. `NULL/""` = the default rule (production devices, FULL); register one rule per device model; **multiple units of the same model share one rule — natively supported** — the built-in monitoring thread spawns an independent session thread for every newly discovered device (present at startup or hot-plugged), up to `um_set_device_limit` (default 4, range 1~8; 8 devices online globally).
- **Multiple models of one family**: `um_usb_add_match(rule_id, &m)` appends a VID/PID match entry to a rule (vid/pid must be non-zero; iface/report_id act per entry); must be called before init.
- **Per-device independent pipelines (v0.4.0)**: every online device (FULL and RAW rules alike) gets its own parsing thread — keys/audio decoding/SN/DPI/meeting fully parsed, no master/slave distinction; with several FULL devices, each one's voice typing, meeting and DPI work completely. The voice audio source is first-come-first-served latched by default (end of this section); meetings are globally exclusive (see 5.6). Devices discovered beyond a rule's limit silently stay offline.
- **Disconnect semantics** (both USB forms handled by existing mechanisms): dongle unplugged = read failure = disconnect; mouse powered off / switched away with dongle present = silent-timeout disconnect, and that endpoint fast-retries (~500 ms) waiting for the device (endpoints that never answered use exponential backoff to avoid handshake storms).
- **`connected` fires only after SN identification completes** (adaptive window: 3 s without frames / up to 8 s with frames; the SDK actively sends SN queries during the window; on timeout it falls back to a transport-level temporary identity `"usb-xxxxxxxx"` / `"ble-<mac>"`, stable only within the current connection session). Callback sequence example:

```
plug in device → (SDK probes/handshakes/identifies) → um_on_connected(SN, UM_CONN_USB)
               → um_on_message(SN, {"type":"deviceInfo","sn":...})
device switches to Bluetooth → um_on_disconnected(SN, UM_CONN_USB) → um_on_connected(SN, UM_CONN_BLE)
unplug device → um_on_disconnected(SN, UM_CONN_BLE)
```

**SN extraction for RAW rules**: the SDK does not parse custom protocols — register an extraction callback (before init):

```c
/* During the identification window the SDK feeds every received raw frame to this callback;
 * write the SN into out_sn and return 1 when identified. The SN must be unique per device —
 * never return a model number (two units would be merged into one). */
typedef int (*um_device_sn_probe)(const uint8_t* data, int len, char* out_sn, int out_cap);
void um_register_device_sn_probe(const char* rule_id, um_device_sn_probe cb);
```

Without registration, devices of that rule come online after the 3 s timeout with a temporary identity (`um_on_raw_frame` still works).

**Per-rule handshake/matching callbacks** (distinguish device types across rules; the suffix-less versions bind the default rule):

```c
typedef int (*um_usb_build_probe_ex)(const char* rule_id, uint8_t* out, int max_len);
typedef int (*um_usb_validate_resp_ex)(const char* rule_id, const uint8_t* buf, int len);
typedef int (*um_ble_match_device_ex)(const char* rule_id, const char* name, const char* address);
void um_register_usb_build_probe_for(const char* rule_id, um_usb_build_probe_ex cb);
void um_register_usb_validate_resp_for(const char* rule_id, um_usb_validate_resp_ex cb);
void um_register_ble_match_device_for(const char* rule_id, um_ble_match_device_ex cb);
```

**Callback thread contract**: all event callbacks (connected/disconnected/message/audio/raw_frame) fire on the SDK's **single internal event-dispatch thread** — globally ordered, **never entered concurrently** (multi-device events included). Don't block inside callbacks and don't call `um_sdk_close` from them; pointer parameters are valid only during the callback.

**Queue health reporting** (v0.3.0): when the host consumes too slowly, the SDK first evicts the oldest audio/raw frames (real-time stream relief; discrete events are never dropped); while eviction is happening it reports via `um_on_message` in a rate-limited way (at most once per 5 s, **device_id = empty string for a global event**):

```json
{"type":"queueOverflow","evicted":12,"dropped":0}
```

`evicted` = audio/raw frames evicted this period (normal protection, stops when consumption resumes); `dropped` = discrete events discarded (non-zero only if the host is stuck for a long time — investigate the consumer).

**Voice audio source** (single audio source for voice sessions across devices; switchable at runtime):

```c
/* Selected device wins: only that device's audio is delivered via um_on_audio, others are muted
 * in the SDK layer. NULL/"" = first-come-first-served latching (default since v0.4.1): the first
 * device to produce audio holds the source until its capture segment ends; the next segment
 * starts first-come-first-served again. Note that NULL does NOT mean "all devices deliver audio".
 * Online status is not validated; applies automatically once the device appears. */
int um_set_audio_source(const char* device_id);
const char* um_get_audio_source(void);   /* empty string = default first-come-first-served policy */
```

**A complete example** is `demo/demo3.c` in this repository (default rule FULL + a second RAW rule simultaneously, SN extraction, device enumeration and per-id commands).

---

## 6. Event Protocol (JSON of `um_on_message`)

All discrete events use the `{"type":"...", ...}` structure. **Extensibility core**: future hardware upgrades only add a `type` or fields; the structure stays stable.

### 6.1 Key Event `keyEvent` (core)

```json
{"type":"keyEvent","key":"<key>","action":"<action>","index":<N>}
```

| Field | Description |
|---|---|
| `key` | physical key name: `speech` / `translation` / `m` / `ai` / `capture` / `raw` |
| `action` | action: `down` / `up` / `hold` / `click` |
| `index` | **M keys only**, which M key (from 1); 0 for other keys |
| `code` | **raw keys only** (v0.4.17): raw key value (decimal value of protocol data[5]) |

**Key × action matrix:**

| Key (`key`) | Actions (`action`) | Notes |
|---|---|---|
| `speech` | `down` / `up` | speech key |
| `translation` | `down` / `up` | translation key |
| `m` | `down` / `hold` / `up` | M key; `index` says which one |
| `ai` | `down` / `up` / `click` | AI key |
| `capture` | `click` | capture (screenshot) key |
| `raw` | `down` / `up` | **passthrough of key types the SDK doesn't know** (v0.4.17): `action` inferred from protocol pairing convention (odd code = down, even code = up); integrators may pair/define semantics by `code`. Heartbeat/battery/DPI/meeting codes are never reported |

**Examples:**

```json
{"type":"keyEvent","key":"speech","action":"down"}
{"type":"keyEvent","key":"m","action":"hold","index":1}
{"type":"keyEvent","key":"ai","action":"click"}
```

> Business example: on `{"key":"speech","action":"down"}` start recording; on `action:"up"` stop and send to recognition; on `{"key":"m","action":"hold","index":1}` trigger whatever your first M key means (screenshot / voice typing / Enter — your choice).

### 6.2 Device Info `deviceInfo`

Reported with heartbeats/audio; contains serial number, battery, connection type, meeting status:

```json
{"type":"deviceInfo","sn":"000AEB20","battery":50,"deviceType":"usb","status":"normal"}
```

| Field | Description |
|---|---|
| `sn` | device serial number (hex string) |
| `battery` | battery percentage (0–100, clamped) |
| `deviceType` | connection type: `"usb"` or `"ble"` |
| `status` | meeting status (v0.4.11, always present): `"normal"` / `"meeting"`. Set to `meeting` when a meeting is created; returns to `normal` when the meeting ends normally **or when no audio flows for 3 consecutive seconds during a meeting** (device reset / silent exit with the link still up — decided by the SDK's stream watchdog). The watchdog **does not emit an extra `meetingDestroyed`** — after an abnormal termination the host decides whether to resume (call `um_meeting_create` again) or finish; the host perceives the fallback via `status`. Pausing with `um_meeting_pause` exempts the watchdog. |

### 6.3 DPI Broadcast `dpiChanged`

```json
{"type":"dpiChanged","level":2}
```

| Field | Description |
|---|---|
| `level` | level byte reported by firmware (encoding varies by firmware family) |

> ⚠️ This event is **unrelated** to `um_set_dpi` — firmware does not acknowledge software DPI setting (zero feedback observed after sending). Only three triggers exist: hardware DPI key, connection established, transport mode switched; 2–3 consecutive frames are common (duplicates are normal). Verify software settings by cursor speed.

### 6.4 Meeting State

```json
{"type":"meetingCreated"}
{"type":"meetingDestroyed"}
{"type":"meetingBusy","owner":"<owner SN>"}
```

`meetingBusy`: this device's meeting command was intercepted (the meeting is globally owned by `owner`; the command return value is also 0).

### 6.5 SDK Warnings `warning` (v0.4.6, unified envelope)

Empty-string `device_id` = global event. `text` is a ready-to-display complete sentence (Chinese); handle by `code`:

```json
{"type":"warning","code":"pairingBroken","text":"…","data":{"endpoint":"…","address":"…","name":"…"}}
```

| `code` | Meaning | `data` |
|---|---|---|
| `pairingBroken` | BLE pairing/encryption state broken (delete and re-pair the device to heal; the endpoint is isolated and backed off meanwhile) | `{endpoint,address,name}` |
| `deviceCountHigh` | total online device count above threshold (default >6) | `{total,limit}` |
| `bleLinkHigh` | online BLE link count above threshold (default >1, i.e. from the 2nd link) — concurrent links squeeze RF scheduling; some mouse firmware degrades voice delivery from the 2nd active link; reduce links or switch to USB/2.4G | `{ble,limit}` |

Each threshold violation is reported once (latched against duplicates); after falling back inside the limit, the next violation reports again.

### 6.6 Queue Health `queueOverflow`

Empty-string `device_id` = global event. When the host consumes too slowly the SDK evicts the oldest audio/raw frames first (discrete events are never dropped); rate-limited reporting while eviction is active (at most once per 5 s):

```json
{"type":"queueOverflow","evicted":12,"dropped":0}
```

`evicted` = audio/raw frames evicted this period (normal protection; stops once consumption resumes); `dropped` = discrete events discarded (non-zero only when the host is stuck for a long time — investigate the consumer).

---

## 7. Audio Data (`um_on_audio`)

The output is selected by `um_set_audio_output_mode`:

### PCM mode (`UM_AUDIO_PCM`, default)

| Property | Value |
|---|---|
| Encoding | raw PCM (no container header) |
| Sample rate | 16000 Hz |
| Bit depth | 16-bit signed little-endian |
| Channels | mono |
| Data rate | 32 kB/s |

- Each callback delivers `pcm` pointing to a decoded segment of `len` bytes;
- **The pointer is valid only during the callback** — copy immediately to keep it;
- To verify: write the stream to `xxx.pcm` and import in Audacity as raw data with `Signed 16-bit PCM / Little-endian / Mono / 16000 Hz`.

### RAW mode (`UM_AUDIO_RAW`)

- `pcm` delivers **raw encoded accumulated blocks** (256 bytes each, undecoded); the format depends on the audio chip (ADPCM/SBC);
- final decoding is done by the caller inside the `um_on_audio` callback.

---

## 8. Platform Notes

### 8.1 Windows

- **Runtime dependencies**: the package already contains everything (FFmpeg `avcodec-58.dll` / `avutil-56.dll` and VC++ runtime copies) — keep them in the same directory as `uMouseSdk.dll`. The DLL directory is **not** on the default search path — loading notes (PATH prepend / `LoadLibraryExW` + `LOAD_WITH_ALTERED_SEARCH_PATH` / CRT preload order and the System32-precedence trap) are in the **package README**;
- **VC++ runtime**: copies are bundled; if the target machine still reports "VCRUNTIME140.dll not found", install the [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe);
- **BLE**: uses the OS Bluetooth stack (built into Win10/11); no extra driver;
- **SmartScreen / antivirus**: the first DLL load may be flagged — mark as trusted.

### 8.2 Linux (UOS / Kylin / FangDe / generic distros)

> This section applies to the Xinchuang / generic Linux packages obtained through sales (this repository does not publish those packages).

**① USB device access permission (important)**

```bash
echo 'KERNEL=="hidraw*", SUBSYSTEM=="hidraw", ATTRS{idVendor}=="abc9", ATTRS{idProduct}=="ca89", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/70-umouse.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

> Change `idVendor`/`idProduct` for other device models (check with `lsusb`). The Bluetooth (BlueZ) channel is not affected.

**② Runtime dependencies**: dynamically links `libavcodec`, `libavutil`, `libudev`, `libdbus-1` (package names vary by distribution).

```bash
sudo apt install libavcodec-extra libudev1 libdbus-1-3   # deb-based (libavutil comes with libavcodec)
# or
sudo dnf install ffmpeg-libs systemd-libs dbus-libs       # rpm-based
```

**③ Bluetooth**: requires the BlueZ service; some distros require the user to be in the `bluetooth` group.

### 8.3 macOS

- **Runtime dependencies**: the package already contains the trimmed FFmpeg dylibs (same directory as the main library, `@loader_path` cross-lookup) — **no brew / FFmpeg installation needed**;
- **Permissions**: the first run requests **Bluetooth** permission (USB / 2.4G dongle connections need no permission);
- **Architecture**: this repository's package is a native Apple Silicon ARM64 build; the Intel (x86_64) build is available via sales;
- **Signing/notarization**: include `libuMouseSdk.dylib` (and the bundled FFmpeg dylibs) in your signature.

---

## 9. Threading Model & Best Practices

### Thread conventions

- **All callbacks fire on the SDK's single internal event-dispatch thread** (not your main thread; globally ordered, never concurrent — multi-device included; see the contract in 5.7);
- `um_*` APIs lock the global service internally and may be called from any thread; still, `um_sdk_init` / `um_sdk_close` should be paired by one controlling thread;
- Query APIs (`um_get_device_count` / `um_get_connection_mode`, etc.) are safe to call inside callbacks.

### Never inside callbacks

- Long blocking (synchronous network requests, large file I/O) — it stalls audio/event delivery and drops frames;
- Calling `um_sdk_close()` — deadlock;
- Holding heavy locks that other threads wait on.

### Recommended pattern: copy in the callback, process on a worker

```c
static void on_message(const char* id, const char* json) {
    enqueue(&g_event_queue, strdup(json));   /* just copy and enqueue, return immediately */
}

void* worker_thread(void*) {
    char* json;
    while ((json = dequeue(&g_event_queue)) != NULL) {
        handle_business(json);   /* slow work is fine here */
        free(json);
    }
    return NULL;
}
```

### Memory & lifetime

| Data | Validity | Advice |
|---|---|---|
| `device_id` (callback param) | callback only | `strdup` immediately if kept |
| `json` (on_message) | callback only | `strdup` immediately if kept |
| `pcm` (on_audio) | callback only | `memcpy` immediately if kept |
| `data` (on_raw_frame) | callback only | `memcpy` immediately if kept |
| `um_sdk_version()` return | process lifetime | static string, pointer may be stored |

### Shutdown

```c
um_sdk_close();   /* idempotent; stops threads and frees resources */
/* no callbacks fire after close */
```

---

## 10. FAQ

**Q1: `on_connected` never fires and `um_get_device_count()` stays 0.**
- Check the device is connected via USB or paired over Bluetooth;
- Linux: check the USB permission rule is installed (see 8.2);
- Enable logs with `um_sdk_init(1)` to watch the probing.

**Q2: `on_connected` fires but no audio arrives.**
- Confirm `um_register_audio` is registered;
- Some devices start pushing audio only after a key press (e.g. the speech key);
- Check the battery — low battery may pause audio.

**Q3: C# / Python integration crashes with "access violation".**
- Almost always a garbage-collected callback delegate. Keep callback objects alive for the process lifetime (fields / module-level variables), never register temporaries.

**Q4: Can multiple devices be connected at once?**
- Yes (multi-device model since v0.3.0, per-device pipelines since v0.4.0): units of the same model share one rule — 4 per rule and 8 globally by default (tunable via `um_set_device_limit`, see 5.7).

**Q5: The SDK bundles FFmpeg — any licensing issue for commercial use?**
- FFmpeg is distributed under **LGPLv2.1** dynamically linked; satisfying the "replaceable" obligation is enough for commercial use. **Do not statically link it**, and distribute `COPYING.LGPLv2.1` with the SDK release (this repository root has it).

**Q6: Does the SDK automatically screenshot / do voice on M-key press?**
- No. The SDK only passes `{"type":"keyEvent","key":"m","action":"down","index":1}`; what happens is your decision.

**Q7: Are packages for different Xinchuang Linux systems (UOS / Kylin / FangDe) interchangeable?**
- Not recommended. System library versions differ; swapping may fail to load the dynamic libraries.

**Q8: How do I confirm the SDK loaded correctly?**
- `um_sdk_version()` should return `"0.4.20"`; NULL or a crash means the library was not loaded correctly.

**Q9: When should I use RAW mode?**
- When the hardware protocol differs from the default. If FULL can't parse it → use RAW passthrough and parse frames yourself.

---

## 11. Version & Licensing

### Version

- Current SDK version: **0.4.20** (query at runtime with `um_sdk_version()`);
- Upgrades keep the public C ABI backward compatible; new capabilities arrive as new JSON `type`s or new exported functions.

### Third-Party Components & Licenses

| Component | Purpose | License | File |
|---|---|---|---|
| FFmpeg (libavcodec/libavutil) | SBC / ADPCM audio decoding | LGPLv2.1 (dynamically linked) | `COPYING.LGPLv2.1` |
| hidapi | cross-platform USB HID access | BSD-3-Clause | `LICENSE.hidapi-bsd` |

- FFmpeg is LGPL and distributed as **dynamic libraries** — **integrators must not switch to static linking**;
- Full third-party notices: `THIRD_PARTY_LICENSES.txt` shipped with the SDK release (this repository root).

### Proprietary Hardware Protocol

The device's communication protocol is proprietary to the vendor; this SDK is authorized for external use. Integrators must not reverse the proprietary protocol for other purposes.

---

## Appendix A: Complete C Example (FULL mode, audio to file)

```c
#include "umouse_sdk.h"
#include <stdio.h>
#include <string.h>

static FILE* g_pcm = NULL;
static long  g_pcm_bytes = 0;

static void on_connected(const char* id, int mode) {
    printf("[connected] %s via %s\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}
static void on_disconnected(const char* id, int mode) {
    printf("[disconnected] %s via %s\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}
static void on_message(const char* id, const char* json) {
    printf("[event] %s %s\n", id, json);
}
static void on_audio(const char* id, const uint8_t* pcm, int len) {
    (void)id;
    if (g_pcm && pcm && len > 0) {
        fwrite(pcm, 1, (size_t)len, g_pcm);
        g_pcm_bytes += len;
    }
}

int main(void) {
    printf("SoniCloud Smart Mouse SDK demo, version %s\n", um_sdk_version());

    g_pcm = fopen("out.pcm", "wb");

    um_register_connected(on_connected);
    um_register_disconnected(on_disconnected);
    um_register_message(on_message);
    um_register_audio(on_audio);

    um_sdk_init(1);

    printf("running... press Enter to exit.\n");
    getchar();

    um_sdk_close();

    if (g_pcm) {
        fclose(g_pcm);
        printf("PCM written to out.pcm, %ld bytes in total.\n", g_pcm_bytes);
    }
    return 0;
}
```

## Appendix B: JSON Event Quick Reference

| `type` | Trigger | Fields | Example |
|---|---|---|---|
| `keyEvent` | any key | `key`,`action`,`index?`,`code?` | `{"type":"keyEvent","key":"speech","action":"down"}` |
| `deviceInfo` | heartbeat/audio report | `sn`,`battery`,`deviceType`,`status` | `{"type":"deviceInfo","sn":"000AEB20","battery":50,"deviceType":"usb","status":"normal"}` |
| `dpiChanged` | DPI broadcast (hardware key / connection / mode switch; unrelated to setDpi) | `level` | `{"type":"dpiChanged","level":2}` |
| `meetingCreated` | meeting created | — | `{"type":"meetingCreated"}` |
| `meetingDestroyed` | meeting ended | — | `{"type":"meetingDestroyed"}` |
| `meetingBusy` | meeting command intercepted (global exclusivity) | `owner` | `{"type":"meetingBusy","owner":"…"}` |
| `warning` | SDK warning (unified envelope, see 6.5) | `code`,`text`,`data` | `{"type":"warning","code":"pairingBroken","text":"…","data":{…}}` |
| `queueOverflow` | host consuming too slowly (rate-limited) | `evicted`,`dropped` | `{"type":"queueOverflow","evicted":12,"dropped":0}` |

**`keyEvent` key quick reference:**

| `key` | Meaning | Available `action` |
|---|---|---|
| `speech` | speech key | `down`,`up` |
| `translation` | translation key | `down`,`up` |
| `m` | M key (`index` says which) | `down`,`hold`,`up` |
| `ai` | AI key | `down`,`up`,`click` |
| `capture` | capture key | `click` |
| `raw` | passthrough of unknown key types (v0.4.17, with `code`) | `down`,`up` |

## Appendix C: API Quick Reference

### Lifecycle

| Function | Returns | Description |
|---|---|---|
| `um_sdk_init(int debug)` | void | initialize and start the SDK |
| `um_sdk_close(void)` | void | stop and release resources (idempotent) |
| `um_sdk_version(void)` | `const char*` | version string, e.g. "0.4.20" |
| `um_enable_crash_report(int enable)` | void | crash recorder switch (on by default) |

### Configuration

| Function | Returns | When |
|---|---|---|
| `um_set_audio_output_mode(int mode)` | 1/0 | any time |
| `um_set_work_mode(int mode)` / `um_set_work_mode_for(rule, mode)` | 1/0 | before init |
| `um_set_device_limit(rule, n)` | 1/0 | before init |
| `um_set_usb_config(rule, cfg)` / `um_usb_add_match(rule, m)` | 1/0 | before init |
| `um_set_ble_config(rule, cfg)` | 1/0 | before init |
| `um_set_channel_pref(int pref)` / `um_set_channel_pref_for(id, pref)` | 1/0 | after init |
| `um_set_audio_source(id)` / `um_get_audio_source(void)` | 1/0 / string | any time |

### Callback registration (all void, call before init)

`um_register_connected` / `um_register_disconnected` / `um_register_message` / `um_register_audio` / `um_register_raw_frame` / `um_register_usb_build_probe(_for)` / `um_register_usb_validate_resp(_for)` / `um_register_ble_match_device(_for)` / `um_register_device_sn_probe`

### Queries (synchronous)

| Function | Returns |
|---|---|
| `um_get_device_count(void)` | online device count (multi-device) |
| `um_get_device_id(int index)` | device id or NULL |
| `um_get_connection_mode(id)` | 0=USB / 1=BLE / -1=not connected |

### Commands (return 1=sent / 0=failed)

| Function | Description |
|---|---|
| `um_write_raw(id, data, len)` | write raw bytes |
| `um_set_dpi(id, level)` | set DPI level (level = firmware level byte, see 5.6) |
| `um_meeting_create(id)` | create a meeting |
| `um_meeting_destroy(id)` | end the meeting |
| `um_meeting_pause(id)` | pause the meeting |
| `um_meeting_resume(id)` | resume the meeting |

---

*If you run into problems, please attach the logs enabled by `um_sdk_init(1)`, the target platform and the SDK version (`um_sdk_version()`) when reporting.*
