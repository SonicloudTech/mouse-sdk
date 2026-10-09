/*
 * umouse_sdk.h —— 声云智能鼠标 SDK 对外唯一头文件（纯 C ABI）
 *
 * 交付形态：一个动态库（uMouseSdk.dll / libuMouseSdk.so / libuMouseSdk.dylib）+ 本头文件。
 * SDK 打通 USB / BLE 传输，解析设备上报协议，输出：
 *   - 原始按键事件（键 + 动作，不含任何业务映射）
 *   - 设备信息 / DPI / 会议等离散事件
 *   - PCM 音频（16kHz / 16bit / 单声道）
 * 业务逻辑（M 键做什么、音频如何识别等）由调用方自行实现。
 *
 * 设计要点（扩展性）：所有离散事件统一走 um_on_message 的 JSON 字符串上报，
 * 未来硬件升级只需新增 JSON type / 字段，本 ABI 保持稳定。
 *
 * ============================ 多设备模型 ============================
 * SDK 支持同时接入多台设备（v0.3.0 起）：
 *   - 设备身份 = SN（设备序列号，由 SDK 在握手识别阶段提取）。
 *     同一台设备的 USB / BLE 只是它的两种连接方式，同一时间只激活一条，
 *     切换时设备身份连续（device_id 不变，conn_mode 属性变化）。
 *   - "设备类型"用规则（rule）描述：um_set_usb_config / um_set_ble_config /
 *     um_set_work_mode_for 的第一个参数 rule_id 注册一条规则（NULL/"" = 生产
 *     默认规则）。不同型号的设备各注册一条规则；同型号多台共用一条规则，
 *     上限由 um_set_device_limit 控制（默认 4，上限 8）。
 *   - 每台设备独立解析（v0.4.0 起）：协议状态机 / 音频解码 / 录音段落盘对
 *     各设备对称，多台 FULL 设备同时语音互不影响；音频上抛单源输出（v0.4.1
 *     起）：um_set_audio_source 选定设备优先，未选定时先到先得——首个出音频
 *     的设备占用本源直到其单次采集结束，期间其余设备音频静音。
 *   - connected 事件在 SN 识别完成后才上抛（识别窗口自适应：无帧 3s / 有帧
 *     最长 8s——设备响应慢时放宽；超时回退传输层临时身份 "usb-xxxx" /
 *     "ble-<mac>"，识别期间 SDK 主动下发 SN 查询）。RAW 规则的 SN 提取需注册
 *     um_register_device_sn_probe（SDK 不解析自定义协议）。
 *
 * 线程约定：所有回调在 SDK 内部唯一的事件派发线程触发，全局有序、
 * 永不并发进入（多设备事件也不会同时回调）。回调内请勿阻塞、勿反调
 * um_sdk_close。指针参数仅在回调期间有效，需要留存的数据请立即拷贝。
 * um_* 接口可从任意线程调用（内部对全局服务指针加锁）；um_sdk_init /
 * um_sdk_close 语义上仍建议由同一控制线程配对调用。回调内可安全调用
 * 查询类接口（um_get_device_count / um_get_connection_mode 等）
 */
#ifndef UMOUSE_SDK_H
#define UMOUSE_SDK_H

#include <stdint.h>

#ifdef _WIN32
    #ifdef UMSDK_EXPORTS
        #define UMSDK_API __declspec(dllexport)
    #else
        #define UMSDK_API __declspec(dllimport)
    #endif
#else
    #define UMSDK_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 连接方式 */
#define UM_CONN_USB 0
#define UM_CONN_BLE 1

/* ============================ 生命周期 ============================ */
/*
 * 初始化并启动 SDK
 * 注册回调应在本调用之前完成，以免错过早期事件。
 * debug 非 0 时开启内部日志。
 */
UMSDK_API void um_sdk_init(int debug);

/* 停止并释放 SDK 内部资源（幂等）。 */
UMSDK_API void um_sdk_close(void);

/* 返回 SDK 版本串，如 "0.2.0"。 */
UMSDK_API const char* um_sdk_version(void);

/* 进程级崩溃记录开关（默认开）。SDK 初始化时安装、关闭时卸载。
 * 开启时：进程发生未处理崩溃（Windows 未处理异常 / POSIX SIGSEGV 等），
 * 在进程当前工作目录写 umouse_crash-<时间戳>.log，内容含异常码、故障地址、
 * 故障模块、原始调用栈与 SDK 版本，保留最近 10 份（超量旧日志在下次
 * um_sdk_init 时清理）。仅技术诊断信息，不含 token/SN 等业务数据。
 * enable=0 关闭。可在 um_sdk_init 前或运行中随时调用，立即生效。 */
UMSDK_API void um_enable_crash_report(int enable);

/* ============================ config配置 ============================ */

/* 设置音频输出模式。
 * 仅影响 um_on_audio 回调内容：PCM=解码后音频，RAW=原始编码累积块（每 256B 一块）。
 * mode 取 UM_AUDIO_PCM / UM_AUDIO_RAW。
 * 返回 1=已设置，0=参数非法。可在 um_sdk_init 之后随时切换。 */
#define UM_AUDIO_PCM 0   /* 内置解码为 PCM：16kHz / 16bit / 单声道（默认） 支持ADPCM/SBC*,音频在um_on_audio回调中处理 */
#define UM_AUDIO_RAW 1   /* 原始编码累积块：未经解码，最终解码请使用方在um_register_audio回调中处理 */
UMSDK_API int um_set_audio_output_mode(int mode);

/* ---- 语音音频源（v0.3.0 多设备，运行中可随时切换；v0.4.1 起为出口默认策略）----
 * device_id 为目标设备的 SN：设置后仅该设备的音频经 um_on_audio 上抛，其它
 * 设备的音频在 SDK 层过滤不上抛——语音识别类会话同一时刻只应有一个音频源。
 * NULL/空串 = 回到默认策略（先到先得）：首个出音频的设备占用本源，直到其
 * 单次采集结束（语音键抬起收尾 / 设备离线），期间其它设备静音；下一次采集
 * 重新先到先得。不校验设备是否在线：设备上线后自动适用（便于宿主在启动期
 * 恢复用户偏好）。
 * 返回 1=已设置。 */
UMSDK_API int um_set_audio_source(const char* device_id);
/* 查询当前选定的音频源（空串 = 默认先到先得策略）。返回指针在下次设置前有效。 */
UMSDK_API const char* um_get_audio_source(void);
/* ---- 工作模式 ----
 * FULL：完整协议解析，SDK 解析按键/音频/SN/DPI/会议，经 um_on_message/um_on_audio 上报。
 * RAW ：仅识别模式，SDK 不做任何协议解析，仅经 um_on_raw_frame 透传原始帧，上位机自行解析。适用于硬件协议与默认不同的设备。
 * 硬件协议相同→FULL；音频芯片不同→RAW+um_set_audio_output_mode(UM_AUDIO_RAW)；
 * 硬件协议不同→RAW 并在 um_on_raw_frame 回调中自行处理，配合 um_write_raw 下发。
 * 每条规则各自设置工作模式。设置须在 um_sdk_init 之前调用（init 时锁定模式，
 * 描述硬件协议形式）；init 之后返回 0。默认规则默认 FULL；非默认规则默认 RAW
 * （自定义设备不走生产协议解析）。
 * 限制：FULL 规则全局只能有一条（内置协议解析为进程级单份）。
 */
#define UM_MODE_FULL 0   /* 完整协议解析（默认规则默认值） */
#define UM_MODE_RAW  1   /* 仅识别：透传原始帧，不做协议解析（非默认规则默认值） */
UMSDK_API int um_set_work_mode(int mode);
/* 指定规则设置工作模式（um_set_work_mode = 默认规则）。
 * rule_id 为 NULL/"" 时作用于默认规则。注册第二条 FULL 规则返回 0。
 * 返回 1=已设置，0=参数非法/重复 FULL/SDK 已启动。 */
UMSDK_API int um_set_work_mode_for(const char* rule_id, int mode);

/* ---- 同型号多台 ----
 * 设置一条规则最多同时接入的设备台数（同型号多只鼠标/接收器共用一条规则）。
 * 须在 um_sdk_init 之前调用；n 范围 1~8（默认 4），越界取边界。
 * v0.4.0 起每台在线设备派生独立解析管线（按键/音频/SN/DPI/会议全解析，
 * 无"解析台/溢出台"之分）；达到上限后新发现的设备不再派生会话（静默不上线）。
 * 语音音频源默认先到先得锁存（um_set_audio_source 注释）；会议全局独占
 * （见下方【会议独占】，越权指令上抛 meetingBusy）。
 * 返回 1=已设置，0=SDK 已启动。 */
UMSDK_API int um_set_device_limit(const char* rule_id, int max_devices);

/* ---- USB（2.4G 接收器）识别标识 ---- */
struct um_usb_config {
    unsigned short vid;         /* 厂商 ID；填 0 保留生产默认 0xABC9 */
    unsigned short pid;         /* 产品 ID；填 0 保留生产默认 0xCA89 */
    int iface;                  /* 数据接口号；<0 表示不限接口（取第一个匹配 VID/PID 的） */
    unsigned char report_id;    /* HID report ID；填 0 保留生产默认 0x0A */
};

/* ---- 蓝牙识别标识 ---- */
struct um_ble_config {
    const char* name;           /* 设备名精确匹配；NULL/空串保留默认 "uMouse" */
    const char* mac_prefix;     /* MAC 前缀匹配（大小写不敏感）；NULL/空串保留默认 "C0:88" */
    const char* service;        /* GATT service UUID；NULL/空串保留默认 */
    const char* characteristic; /* GATT notify 特征 UUID；NULL/空串保留默认 */
};

/* 覆盖 2.4G USB 接收器识别标识（按规则注册，须在 um_sdk_init 之前调用）。
 * rule_id 为 NULL/"" 时作用于默认规则（生产设备）；非空则为该 rule_id 注册
 * 或更新一条新规则（不同型号设备各一条规则）。init 之后调用返回 0（不生效）。
 * vid/pid/report_id 为 0 时保留对应生产默认值；iface 按字面生效——>=0 指定
 * 数据接口号，<0 不限接口，无"0 保留默认"语义（生产默认 iface=1 属旧设备；
 * memset 清零后只填 vid/pid 会使接口号静默变为 0，接入新设备务必显式填写）。
 * 本调用为单匹配整表替换语义（规则的匹配列表重置为这一条）。
 * cfg 指针仅本次调用期间读取。
 * 返回 1=已采纳，0=未采纳（cfg 为空，或 SDK 已启动）。 */
UMSDK_API int um_set_usb_config(const char* rule_id, const struct um_usb_config* cfg);
/* 追加一条 VID/PID 匹配项（同族多型号：一个规则匹配多个 VID/PID 列表）。
 * 须在 um_sdk_init 之前调用；m 的 vid/pid 必须非 0（0 的"默认"语义只在
 * um_set_usb_config 里存在），iface/report_id 按字面生效（追加项各带各的）。
 * 返回 1=已追加，0=未采纳（m 为空 / vid/pid 为 0 / SDK 已启动）。 */
struct um_usb_match {
    unsigned short vid;         /* 厂商 ID（必填） */
    unsigned short pid;         /* 产品 ID（必填） */
    int iface;                  /* 数据接口号；>=0 指定，<0 不限 */
    unsigned char report_id;    /* HID report ID（该匹配项的会话内生效） */
};
UMSDK_API int um_usb_add_match(const char* rule_id, const struct um_usb_match* m);
/* 覆盖蓝牙识别标识（按规则注册，语义同 um_set_usb_config，仅 init 前生效）。
 * cfg 内字符串指针仅本次调用期间读取，SDK 内部立即拷贝，调用方随后可释放原串。
 * 返回 1=已采纳，0=未采纳。 */
UMSDK_API int um_set_ble_config(const char* rule_id, const struct um_ble_config* cfg);

/* ---- 通道偏好（运行时可改，per 设备）----
 * AUTO   ：USB 优先，USB 静默超时后转 BLE（默认，当前行为）。
 * USB    ：仅探测 USB（2.4G 接收器），永不转 BLE。
 * BLE    ：仅探测 BLE，跳过 USB 接收器（即使接收器在位、被系统枚举到）。
 * 场景：2.4G 接收器在位但鼠标本体切到蓝牙时，接收器会被枚举到导致 USB 假在线，
 *       此时用 UM_CHAN_BLE 强制后台线程只走蓝牙通道接管。
 * 切换瞬间会断开当前通道并按新偏好重新探测。
 * um_set_channel_pref 作用于全部已连接设备（单设备宿主兼容路径）；
 * um_set_channel_pref_for 作用于指定设备（device_id 为 SN，NULL=唯一在线设备）。
 * 返回 1=已设置，0=参数非法或 SDK 未启动（通道切换为运行时动作，须 init 后调用）。
 */
#define UM_CHAN_AUTO 0
#define UM_CHAN_USB  1
#define UM_CHAN_BLE  2
UMSDK_API int um_set_channel_pref(int pref);
UMSDK_API int um_set_channel_pref_for(const char* device_id, int pref);


/* ============================ 回调类型 ============================ */

/* 设备连接。conn_mode: UM_CONN_USB / UM_CONN_BLE。 */
typedef void (*um_on_connected)(const char* device_id, int conn_mode);
/* 设备断开。 */
typedef void (*um_on_disconnected)(const char* device_id, int conn_mode);
/* 离散事件，json 为 UTF-8 字符串（见文末协议）。 */
typedef void (*um_on_message)(const char* device_id, const char* json);
/* PCM 音频：16kHz / 16bit / 单声道，小端。pcm 仅回调期间有效。 */
typedef void (*um_on_audio)(const char* device_id, const uint8_t* pcm, int len);

/* 原始帧（RAW 工作模式设备；FULL 规则的"溢出台"——解析台之外的后续设备——
 * 同样触发）。data 为设备上报的原始字节，仅回调期间有效，需留存请立即拷贝。
 * conn: UM_CONN_USB / UM_CONN_BLE。 */
typedef void (*um_on_raw_frame)(const char* device_id, const uint8_t* data, int len, int conn);

/* USB 握手查询指令生成（仅 RAW 工作模式 + USB 通道生效）。
 * 把握手查询字节填入 out，返回字节长度（<= max_len）；返回 <=0 表示用默认查询指令。
 * 默认查询指令：{report_id, 0x03, 0xA0, 0x00}（补零到 10 字节）。 */
typedef int (*um_usb_build_probe)(uint8_t* out, int max_len);
/* 同上，带 rule_id（多规则下区分设备类型）。绑定到具体规则的注册接口用本签名。 */
typedef int (*um_usb_build_probe_ex)(const char* rule_id, uint8_t* out, int max_len);

/* USB 握手回应帧校验（仅 RAW 工作模式 + USB 通道生效）。
 * buf/len 为读回的一帧（len 为实际字节数）。返回 1=有效回应（设备在线），0=不是。
 * 未注册时用默认校验：buffer[0]==report_id && buffer[1]>0x05 && buffer[2]==0x99 && buffer[5]==0x00。 */
typedef int (*um_usb_validate_resp)(const uint8_t* buf, int len);
/* 同上，带 rule_id。 */
typedef int (*um_usb_validate_resp_ex)(const char* rule_id, const uint8_t* buf, int len);

/* BLE 目标设备识别（仅 RAW 工作模式 + BLE 通道生效）。
 * BLE 枚举到系统已连接设备时调用，调用方自行判断是否为目标设备。
 * name=设备 BLE 名称，address=MAC 地址（"XX:XX:XX:XX:XX:XX"）。
 * 返回 1=是目标设备（接管），0=不是（跳过）。
 * 未注册时用默认匹配：名称精确匹配，或 MAC 前缀匹配（mac_prefix，大小写不敏感；
 * mac_prefix 为空时退回历史位掩码规则） */
typedef int (*um_ble_match_device)(const char* name, const char* address);
/* 同上，带 rule_id。 */
typedef int (*um_ble_match_device_ex)(const char* rule_id, const char* name, const char* address);

/* 设备身份（SN）提取（仅 RAW 工作模式生效，按规则注册）。
 * SDK 不解析自定义协议，识别窗口内把收到的每帧原始数据交给本回调；
 * rule_id 标识帧所属规则（多协议并存时区分解析方式）；
 * 识别出 SN 时写入 out_sn（\0 结尾，<= out_cap-1 字节）并返回 1，其余返回 0。
 * 提取到的 SN 即该设备的对外 device_id——必须每台设备唯一；
 * 不唯一（如型号号）会让两台设备被合并为一台。未注册时识别超时（3s）后
 * 回退传输层临时身份（"usb-xxxx" / "ble-<mac>"，仅本次连接会话内稳定）。
 * 在 SDK 读线程触发，勿阻塞。 */
typedef int (*um_device_sn_probe)(const char* rule_id, const uint8_t* data, int len,
                                  char* out_sn, int out_cap);

/* ============================ 回调注册 ============================
 * 事件类回调（connected/disconnected/message/audio/raw_frame）支持运行期注册：
 * 内部原子指针每次调用时加载，注册后下一次事件即生效。
 * 握手/匹配/身份提取类回调（usb_build_probe / usb_validate_resp /
 * ble_match_device / device_sn_probe）仅在 um_sdk_init 时快照注入一次——
 * init 之后注册不生效，必须在 init 前完成。
 * 无后缀版本绑定默认规则；"_for" 版本按 rule_id 绑定（须先经 um_set_usb_config
 * 等注册该规则，或随后注册亦可——快照在 init 时统一收取）。
 */

UMSDK_API void um_register_connected(um_on_connected cb);
UMSDK_API void um_register_disconnected(um_on_disconnected cb);
UMSDK_API void um_register_message(um_on_message cb);
UMSDK_API void um_register_audio(um_on_audio cb);
UMSDK_API void um_register_raw_frame(um_on_raw_frame cb);   /* RAW 规则全帧透传；FULL 规则每台设备亦对称透传（v0.4.0） */
UMSDK_API void um_register_usb_build_probe(um_usb_build_probe cb);       /* RAW 模式 USB 握手查询指令 */
UMSDK_API void um_register_usb_validate_resp(um_usb_validate_resp cb);   /* RAW 模式 USB 握手回应校验 */
UMSDK_API void um_register_ble_match_device(um_ble_match_device cb);    /* RAW 模式 BLE 目标设备识别 */
UMSDK_API void um_register_usb_build_probe_for(const char* rule_id, um_usb_build_probe_ex cb);
UMSDK_API void um_register_usb_validate_resp_for(const char* rule_id, um_usb_validate_resp_ex cb);
UMSDK_API void um_register_ble_match_device_for(const char* rule_id, um_ble_match_device_ex cb);
UMSDK_API void um_register_device_sn_probe(const char* rule_id, um_device_sn_probe cb);  /* RAW 模式 SN 提取 */

/* ============================ 查询（同步） ============================ */

/* 当前已连接（完成识别）的设备数。 */
UMSDK_API int um_get_device_count(void);
/* 取第 index 个已连接设备的 id（SN，或识别超时回退的临时身份）。
 * index 按设备接入顺序；返回的指针在 SDK 生命周期内稳定（um_sdk_close 前有效）。
 * 越界返回 NULL。 */
UMSDK_API const char* um_get_device_id(int index);
/* 连接方式：UM_CONN_USB / UM_CONN_BLE；device_id 为 NULL 时取唯一在线设备，
 * 多台在线时返回 -1（须显式指定）。未知 id 返回 -1。 */
UMSDK_API int um_get_connection_mode(const char* device_id);

/* ============================ 命令 / 请求 ============================ */
/* 返回：1=命令已下发，0=失败（未连接/参数空/被拦截等）。异步结果经 um_on_message 上报。
 * device_id 为目标设备的 SN；NULL 时取唯一在线设备，多台在线时返回 0
 * （会话类操作如会议/DPI/写指令在多设备下必须显式指定设备）。
 *
 * 会议独占：全局同一时刻只允许一台设备处于会议中。会议进行期间只有该设备
 * 可执行会议操作，其它设备的会议指令在 SDK 层拦截（不下发、返回 0），并向
 * 被拦截设备上抛 {"type":"meetingBusy","owner":"<占用者SN>"}。归属随会议
 * 结束（meetingDestroyed）或该设备断开而解除。 */

/* 向指定设备的活动通道写原始字节（两种工作模式均可用）。
 * 适用于 RAW 模式下上位机自行组织协议下发，或 FULL 模式补发自定义命令。
 * 注意：USB-HID 首字节通常需为 report_id（默认 0x0A），由调用方组装。
 * 返回 1=已写入，0=失败（未连接/参数空）。 */
UMSDK_API int um_write_raw(const char* device_id, const uint8_t* data, int len);

/* 设置 DPI 档位。level 为固件相关的档位字节（协议 03 22 <level>，不透明选择子，
 * 非 1..N 索引、非 DPI 数值）：主流固件映射 {800→21, 1200→32, 1600→42, 2400→63,
 * 4000→4, 6000→5}，其它渠道/固件族编码不同（如 0 基索引）。档位↔字节映射由宿主
 * 按渠道/固件类型自建，SDK 只透传下发；表外字节对固件是未定义行为。
 * 返回 1 仅代表字节已写入传输通道——固件对软件设置不回执（2026-09-11 两固件
 * 实测 10 次下发零 0x2E），生效与否需宿主以光标速度等旁证验证。
 * {"type":"dpiChanged","level":N} 事件与软件设置无关：由硬件 DPI 键切档、连接
 * 建立与传输模式切换时的固件状态广播触发（可能连发 2~3 帧；实测连接时 level=2、
 * BLE→USB 切换后 level=0），level 为固件回报的档位字节，原样透传。 */
UMSDK_API int um_set_dpi(const char* device_id, int level);

/* 会议模式（新版）。 */
UMSDK_API int um_meeting_create(const char* device_id);
UMSDK_API int um_meeting_destroy(const char* device_id);
UMSDK_API int um_meeting_pause(const char* device_id);
UMSDK_API int um_meeting_resume(const char* device_id);

#ifdef __cplusplus
}
#endif

/*
 * ======================= um_on_message JSON 协议 =======================
 * 统一结构：{"type":"...", ...}
 *
 * 1) 按键事件（只报键 + 动作，无业务逻辑）
 *    {"type":"keyEvent","key":"<key>","action":"<action>","index":<N>}
 *    key    : "speech" | "translation" | "m" | "ai" | "capture" | "raw"
 *    action : "down" | "up" | "hold" | "click"
 *    index  : M 键专用，第几个 M 键（1 起）；非 M 键为 0
 *    支持组合：
 *      speech      : down / up
 *      translation : down / up
 *      m           : down / hold / up   (index=data[6]，旧帧默认 1)
 *      ai          : down / up / click
 *      capture     : click
 *      raw（v0.4.17）: down / up + "code":<N>——SDK 未接入键型的原始键值
 *             透传（2026-09-10 真机 0x57/58、0x5B/5C、0x45/46、0x47/48 等）。
 *             code=data[5] 十进制原值；action 按协议配对惯例推断（奇码=down、
 *             偶码=up），宿主可按 code 自行配对/覆盖语义。心跳/电量/DPI/会议
 *             等非按键语义码不上抛。
 *
 * 2) 设备信息（随心跳/音频上报）
 *    {"type":"deviceInfo","sn":"000AEB20","battery":50,"deviceType":"usb"}
 *    {"type":"deviceInfo","sn":"000AEB20","battery":50,"deviceType":"usb","status":"meeting"}   ///会议状态   normal / meeting
 *    status（v0.4.11）：会议状态，恒随帧下发（缺省 normal）。SDK 侧推断：
 *    会议建立（created 帧）置 meeting；会议正常结束（destroyed 帧/宿主销毁/
 *    设备断开）或会议中连续 3s 无音频流入（设备复位/静默退出但链路未断，
 *    断流看门狗）回 normal。断流裁决不补发 meetingDestroyed——意外终止的
 *    处置（续会=重新 um_meeting_create / 结束=um_meeting_destroy）由宿主
 *    决策，宿主经 status 回落感知。宿主 pause 期间豁免裁决。旧硬件专用——
 *    新固件显式上报状态后以固件为准。
 * 3) DPI 状态广播（与 um_set_dpi 无关——固件不回执软件设置，见其注释；
 *    仅硬件 DPI 键切档、连接建立与传输模式切换时由固件主动上报，可能连发
 *    2~3 帧；level 为固件回报的档位字节，编码随固件族而异）
 *    {"type":"dpiChanged","level":2}
 *
 * 4) 会议
 *    {"type":"meetingCreated"}
 *    {"type":"meetingDestroyed"}
 *    {"type":"meetingBusy","owner":"<SN>"}   ← 该设备的会议指令被拦截（会议被
 *                                             owner 设备独占，返回值同时为 0。
 *                                             v0.4.0 起每设备独立解析管线，
 *                                             会议冲突仅此一种形态）
 *
 * 5) 队列健康（SDK → 宿主；device_id 为空串 = 全局事件，非设备归属）
 *    {"type":"queueOverflow","evicted":<N>,"dropped":<M>}
 *    evicted = 本周期因宿主消费不及时被淘汰的音频/原始帧数（实时流泄压，
 *              属正常防护，恢复消费即停）；dropped = 被丢弃的离散事件数
 *              （宿主长时间卡死才会出现，须排查消费端）。
 *    丢弃发生期间限流上报（每 5s 至多一条），数值为周期内增量。
 *
 * 6) SDK 告警（统一信封，v0.4.6；device_id 为空串 = 全局事件，非设备归属）
 *    {"type":"warning","code":"<机读码>","text":"<可直接显示的中文提示>","data":{…}}
 *    宿主可把 text 原样弹给用户（已含完整句子），按 code 细分处理/本地化。
 *    当前 code：
 *      pairingBroken   BLE 配对/加密态损坏（删除设备重新配对即愈；期间端点
 *                      已隔离退避，不反复轰炸）。data: {endpoint,address,name}
 *      deviceCountHigh 在线设备总数超限（默认 >6 台）。data: {total,limit}
 *      bleLinkHigh     在线 BLE 链路数超限（默认 >1 条，即第 2 条起）。多活链
 *                      挤占射频调度，Telink/ADPCM 族固件鼠标的语音投递自
 *                      第 2 条活链起劣化，建议减链或改 USB/2.4G。data: {ble,limit}
 *    越界时各上抛一次（闩锁防重复）；回落到限值内后再次越界会重新上抛。
 *    阈值为编译期常量（kLoadWarnTotal / kLoadWarnBle）。
 *
 * 7) 领夹麦（新设备形态：领夹麦鼠标，设备类型 0x04。docx《上报领夹麦状态+
 *    电量协议》，帧走常规 0x0A D0 99 解析通道，指令 0x12/0x13）
 *    {"type":"lavalierMicStatus","link":<0|1|2>,"connected":<true|false>}
 *      领夹麦克风连接状态上报（指令 0x12）。link 指领夹麦与本体间的链路类型：
 *      0=USB、1=2.4G、2=BLE；connected 由从设备状态字节换算（1=已连接、
 *      2=未连接，协议未定义值按未连接处理）。
 *    {"type":"lavalierMicBattery","battery":<0..100>}
 *      领夹麦克风电量上报（指令 0x13），单位 %，SDK 已钳 0..100。
 */

#endif /* UMOUSE_SDK_H */
