# demo 目录说明 —— 各示例的适用场景

本目录是 mouse-sdk 的接入示例集：单文件纯 C，便于复制改造。SDK 二进制在
`packages/` 交付包中——解压对应平台包到仓库根目录（得到 `uMouseSdk/`）即可
编译运行任一示例。

**先按场景选 demo：**

| 你的情况 | 用哪个 |
|---|---|
| 我就是接声云智能鼠标，要按键/音频/设备信息直接能用 | **demo.c** |
| 声云鼠标，但帧我要自己解（或只想要原始流） | **demo2.c** |
| 声云鼠标 + 其它厂商设备**同时**在线，各走各的协议 | **demo3.c** |
| 我的硬件完全不是声云鼠标（USB/BLE 标识与协议全是我的） | **demo4.c** |

---

## demo.c —— FULL 模式·生产设备基础接入

最简可用样例（约 60 行）：不配置任何标识（生产默认值），SDK 完整解析声云
鼠标协议，以结构化事件上抛——`keyEvent`（语音/翻译/AI/M 键…）、`deviceInfo`
（SN/电量）、`dpiChanged`、解码后的 `audio`（PCM）等。演示
`um_sdk_init` → 回调注册 → 事件消费 → `um_sdk_close` 的最小闭环。
**第一次接入从它开始。**

## demo2.c —— RAW 模式·原始帧透传

设备仍是声云生产标识（默认 VID/PID、BLE 服务/特征），但
`um_set_work_mode(UM_MODE_RAW)` 把解析权交回应用：SDK 只做枚举/连接/读写，
原始帧经 `um_register_raw_frame` 透传，demo 内自行解析声云协议
（SN/按键/心跳/音频切片）。同时演示：

- USB 握手自定义（`um_register_usb_build_probe` / `um_register_usb_validate_resp`）；
- BLE 目标设备识别回调（`um_register_ble_match_device`）；
- 交互式通道切换（USB / BLE / AUTO）与 `um_write_raw` 下发查询。

适合：需要在 SDK 解析之外拿到字节级原始流、或按自己的规则解读声云帧的场景。

## demo3.c —— 多设备多规则·FULL 与 RAW 共存

两类设备同时在线：规则 A（默认规则，不配置即生产设备，FULL 全解析）+
规则 B（`rule_id="ext"`，外部设备 RAW 模式，注册独立 USB/BLE 标识、握手回调、
SN 提取回调）。演示多设备核心契约：

- `device_id` = SN（FULL 设备 SDK 内部识别；RAW 设备经
  `um_register_device_sn_probe` 提取）；同一设备 USB/BLE 切换身份不变；
- `um_get_device_count` / `um_get_device_id` / `um_get_connection_mode` 枚举；
- 多设备在线时 `um_write_raw` 等会话命令**必须显式指定 device_id**；
- 所有回调在 SDK 唯一派发线程触发，全局有序、不会并发进入。

## demo4.c —— 完全的外部硬件接入·完整配方

**把一块对 SDK 完全陌生的硬件从零接入的完整参考**。默认规则被整体替换为
目标硬件的自定义标识——本程序只认你的硬件。

**★ 硬件层前提（不满足则 SDK 无法接管，公共 API 无旁路）**：

1. **数据通道必须是所指接口的第一个 HID 顶层集合（collection），或独占一个
   接口**——接口下挂多个集合时 SDK 只接管第一个集合（如 Col01 标准鼠标 +
   Col02 vendor 数据的设备，数据在 Col02 就接不进来；`iface` 只能过滤接口号，
   无法指定集合）；
2. **数据通道必须支持输出报告写入**（Output Report 或 Feature Report 任一）——
   USB 握手要先写探针，写成功才开 3.5s 读窗；两条写路径都被拒的端点直接判
   不可用；
3. 建议（非硬性）：设备提供心跳/自发帧，静止状态也能完成识别。

（契约原文见包内 `uMouseSdk/include/umouse_sdk.h`【外部硬件接入的硬件层前提】。）

「外部硬件接入的五个必答问题」在文件里逐段落位：

1. **设备长什么样**：USB VID/PID/接口/report id + BLE 名称/MAC/服务/特征
   （文件头 `HW_*` 宏）；
2. **怎么确认在位**：握手探针 `um_register_usb_build_probe` + 回应帧签名
   `um_register_usb_validate_resp`；
3. **身份是什么**：`um_register_device_sn_probe` 回调从识别窗口的帧里提取
   每台唯一的 SN（提取不到时设备以临时身份 usb-xxxx 上线）；
4. **帧怎么解析**：`raw_frame` 回调里解目标协议——按键/电量/心跳/会议/
   音频切片，未识别帧进「不同帧登记表」完整留痕（`t` 查看、`a` 逐帧全打）；
5. **怎么下发命令**：`um_write_raw`（`w` 命令，默认发 SN 查询）。

为「开箱即可真机验证」，当前载体用声云 uMouse 的真实标识与协议（0x99 体系）
——即把生产设备当作一块外部硬件从零接入一遍；接你自己的硬件时替换 `HW_*`
宏与解析函数即可，各回调接法不变。

> 与生产设备共存？demo4 演示的是「默认规则整体替换」（只认外部硬件）；
> 若要声云设备与你的设备同时在线，用 demo3 的命名规则写法
> （`um_set_usb_config(rule_id, …)`）把本 demo 的配置挂到独立规则上。

---

## 构建与运行（以 demo4 为例，其余同理）

解压对应平台交付包到仓库根目录后：

```bat
REM Windows（MSVC 开发者命令行 / developer prompt）
cl /I uMouseSdk\include demo\demo4.c /Fe:demo4.exe /link /LIBPATH:uMouseSdk uMouseSdk.lib
set PATH=%CD%\uMouseSdk;%PATH%
demo4.exe
```

```bash
# macOS
cc -IuMouseSdk/include demo/demo4.c -LuMouseSdk -luMouseSdk -o demo4 && ./demo4
```

（Windows 运行时依赖 `uMouseSdk\` 内随附的 FFmpeg/VC 运行库 DLL，与库同目录
即可找到。）

## 通用说明

- **回调线程契约**：所有回调（raw_frame/message/audio/connected…）在 SDK
  唯一事件派发线程触发，全局有序；回调内不要阻塞或做重活。
- **EOF 防护**：demo 的交互循环对 stdin 关闭（管道/GUI 拉起）做了退出处理，
  嵌入自己工程时可去掉交互、按事件驱动组织。
