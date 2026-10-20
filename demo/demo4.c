/*
 * demo4.c —— 完全的外部硬件接入示例·完整配方（纯 C，USB + BLE 全自定义）
 *
 * 定位：把「一块对 SDK 完全陌生的硬件」从零接入的完整参考。SDK 退化为
 * 纯传输层（枚举、打开、握手调度、读写、断线重连、通道偏好），协议的全部
 * 语义由本文件自带——换真实第三方硬件时逐段替换即可。
 *
 * 外部硬件接入的五个必答问题（本文件逐段落位）：
 *   1. 设备长什么样 —— USB VID/PID/接口/report id；BLE 名称/MAC/服务/特征
 *                      （下方 HW_* 宏，§配置）。
 *   2. 怎么确认在位 —— 握手探针 build_probe + 回应帧签名 validate_resp（§握手）。
 *   3. 身份是什么   —— sn_probe 回调：从识别窗口的帧里提取每台唯一的 SN
 *                      （§SN 提取；提取不到时设备以临时身份 usb-xxxx 上线）。
 *   4. 帧怎么解析   —— raw_frame 回调里解目标协议（§解析：按键/电量/心跳/
 *                      会议/音频切片；未识别帧进「不同帧登记表」完整留痕）。
 *   5. 怎么下发命令 —— um_write_raw（w 命令，默认发 SN 查询）。
 *
 * ★ 硬件层前提（不满足则 SDK 无法接管，公共 API 无旁路；契约详见
 *   umouse_sdk.h【外部硬件接入的硬件层前提】）：
 *   1. 数据通道必须是所指接口的【第一个 HID 顶层集合】或独占一个接口——
 *      接口下挂多个集合时 SDK 只接管第一个集合（iface 只过滤接口号，
 *      选不到集合）；
 *   2. 数据通道必须支持输出报告写入（Output 或 Feature 任一）——握手探针
 *      写成功才开 3.5s 读窗；
 *   建议：设备带心跳帧，静止状态也能完成识别（否则识别依赖用户按键/移动）。
 *
 * 为「开箱即可真机验证」，当前载体用声云 uMouse 的真实标识与协议（0x99
 * 体系）——即把生产设备当作一块外部硬件从零接入。与 demo2 的区别：
 * demo2 是「原始帧快速查看 + 通道切换」；本文件是接入配方全件套（握手、
 * SN 回调、事件/音频/会议解析、下行命令、未知帧登记表）。
 *
 * 注意：USB 稳态读帧在 SDK 通道层钳到 34 字节（kUsbReadLen 截断契约），
 * 超过 34B 的长帧会被截断显示；BLE 帧不受此限。
 *
 * 构建：解压对应平台交付包到仓库根目录后，按 demo/README.md 的一行命令
 * 编译运行（产物与 uMouseSdk 库同目录）。
 */
#include "umouse_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h> 
#endif

/* ============================ §1 外部硬件标识（换设备改这里） ============================
 * 当前为声云 uMouse 实测值。BLE 服务 UUID 同时充当枚举过滤器——只有
 * 「系统已连接且暴露该服务」的设备才会进入候选。 */
#define HW_VID         0xABC9
#define HW_PID         0xCA89
#define HW_IFACE       1                     /* 数据接口号；-1 = 不限 */
#define HW_REPORT_ID   0x0A                  /* HID report id（帧首字节） */

#define HW_BLE_NAME        "uMouse"          /* 设备名（精确） */
#define HW_BLE_MAC_PREFIX  "C0:88"           /* MAC 前缀 */
#define HW_BLE_SERVICE     "0000fff0-0000-1000-8000-00805f9b34fb"
#define HW_BLE_CHAR        "0783b03e-8535-b5a0-7140-a304d2495cb8"

/* ============================ §4 目标协议速查（0x99 体系） ============================
 * 事件帧：[0A][d1>05][99][电量][DPI][命令码][参数…]
 *   命令码：55 心跳(电量在 d3) / 00 电量 / 2E DPI(档在 d4) / 21·22 语音 /
 *   2A·2B 翻译 / 23·24·31 AI / 25 截图 / 35 M键(d6=槽位,d7=27按下/29长按/28抬起) /
 *   9A·9B 会议创建/销毁
 * SN 帧：[0A][0A][8B×8] → 8 字节即 16 位十六进制序列号
 * 音频帧：无统一帧头（四芯片族嗅探；本 demo 识别 Freqchip 标记 d1==0x9C 并计数） */

/* ============================ 统计与「不同帧」登记表 ============================ */
#define MAX_DISTINCT    512                  /* 最多登记多少种不同帧 */
#define MAX_FRAME_BYTES 128                  /* USB 稳态读 ≤34B；BLE 可更长 */
struct frame_rec {
    int           len;
    int           conn;                      /* UM_CONN_USB / UM_CONN_BLE */
    long          count;
    unsigned char data[MAX_FRAME_BYTES];
};
static struct frame_rec g_recs[MAX_DISTINCT];
static int  g_rec_n = 0;
static long g_frames = 0, g_keys = 0, g_audio_pkts = 0, g_audio_bytes = 0;
static int  g_flood  = 0;                    /* 'a' 切换：1 = 逐帧全打 */

static void print_rec(int idx) {
    const struct frame_rec* r = &g_recs[idx];
    printf("%s #%-3d len=%-3d x%-6ld ", r->conn == UM_CONN_BLE ? "[BLE]" : "[USB]",
           idx, r->len, r->count);
    for (int i = 0; i < r->len && i < MAX_FRAME_BYTES; ++i) printf("%02x ", r->data[i]);
    printf("\n");
}

static void print_table(void) {
    printf("---- distinct(unparsed) frames: %d/%d  (total frames %ld) ----\n",
           g_rec_n, MAX_DISTINCT, g_frames);
    for (int i = 0; i < g_rec_n; ++i) print_rec(i);
}

/* 登记一个未识别帧（字节序列相同 = 同一帧型，只累计次数） */
static void record_frame(const unsigned char* data, int len, int conn) {
    int flen = len < MAX_FRAME_BYTES ? len : MAX_FRAME_BYTES;
    for (int i = 0; i < g_rec_n; ++i) {
        struct frame_rec* r = &g_recs[i];
        if (r->len == flen && r->conn == conn && memcmp(r->data, data, (size_t)flen) == 0) {
            ++r->count;
            if (g_flood) print_rec(i);
            return;
        }
    }
    if (g_rec_n < MAX_DISTINCT) {
        struct frame_rec* r = &g_recs[g_rec_n];
        r->len = flen;
        r->conn = conn;
        r->count = 1;
        memcpy(r->data, data, (size_t)flen);
        printf("\n");                       /* 新帧型前空一行，突出显示 */
        print_rec(g_rec_n);
        ++g_rec_n;
    }
}

/* ============================ §2 USB 握手回调 ============================
 * build_probe：构造握手查询（首字节必须是 report id）。此处用电量查询
 *   [0A][03][A0][00]，补零到 10 字节（HID 写建议 ≥8 字节对齐）。
 * validate_resp：校验回应帧签名——命中即在位、接管该端点。此处要求
 *   0x99 事件帧族 + 命令码 0x00（电量应答）：换设备时改成你的帧签名。 */
static int hw_build_probe(unsigned char* out, int max_len) {
    if (max_len < 10) return 0;
    memset(out, 0, 10);
    out[0] = HW_REPORT_ID;
    out[1] = 0x03;
    out[2] = 0xA0;
    out[3] = 0x00;
    return 10;
}

static int hw_validate_resp(const unsigned char* buf, int len) {
    if (len < 6) return 0;
    return buf[0] == HW_REPORT_ID && buf[1] > 0x05 &&
           buf[2] == 0x99 && buf[5] == 0x00;
}

/* ============================ §3 SN 提取回调（身份的唯一来源） ============================
 * 识别窗口内 SDK 把每帧交给你；从「你的协议」里提取每台唯一的序列号。
 * 契约：SN 必须每台唯一——若帧里只有型号号请不要返回它（两台会被并成
 * 一台、互抢会话）；提取不到时设备以临时身份上线（id 形如 usb-xxxx）。 */
static int hw_sn_probe(const char* rule_id, const unsigned char* data, int len,
                       char* out_sn, int out_cap) {
    (void)rule_id;
    /* SN 帧 [0A][0A][8B×8]：8 字节 → 16 位十六进制字符 */
    if (!data || len < 10 || out_cap < 17) return 0;
    if (data[0] != 0x0A || data[1] != 0x0A) return 0;
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 8; ++i) {
        out_sn[i * 2]     = hex[(data[2 + i] >> 4) & 0x0F];
        out_sn[i * 2 + 1] = hex[data[2 + i] & 0x0F];
    }
    out_sn[16] = '\0';
    return 1;
}

/* ============================ §4 应用侧协议解析（RAW 帧的全部语义归宿这里） ============================ */
static const char* key_name(unsigned char code) {
    switch (code) {
        case 0x21: return "语音 down";
        case 0x22: return "语音 up";
        case 0x2A: return "翻译 down";
        case 0x2B: return "翻译 up";
        case 0x23: return "AI down";
        case 0x24: return "AI up";
        case 0x31: return "AI click";
        case 0x25: return "截图 click";
        case 0x27: return "M down";
        case 0x29: return "M hold";
        case 0x28: return "M up";
        default:   return "未知键";
    }
}

/* 返回 1 = 已识别并打印；0 = 未识别（由调用方登记进帧型表） */
static int parse_frame(const unsigned char* data, int len, int conn) {
    if (!data || len < 2) return 0;
    const char* tag = (conn == UM_CONN_BLE) ? "[BLE]" : "[USB]";

    /* SN 上报帧 */
    if (len >= 10 && data[0] == 0x0A && data[1] == 0x0A) {
        char sn[17] = {0};
        static const char hex[] = "0123456789ABCDEF";
        for (int i = 0; i < 8; ++i) {
            sn[i * 2]     = hex[(data[2 + i] >> 4) & 0x0F];
            sn[i * 2 + 1] = hex[data[2 + i] & 0x0F];
        }
        printf("%s [SN  ] %s\n", tag, sn);
        return 1;
    }

    /* 音频帧（Freqchip 标记 d1==0x9C；完整四族嗅探见 SDK FULL 模式） */
    if (data[1] == 0x9C && len > 2) {
        ++g_audio_pkts;
        g_audio_bytes += len - 2;
        if (g_audio_pkts % 200 == 0)
            printf("%s [AUD ] %ld pkts, %ld bytes\n", tag, g_audio_pkts, g_audio_bytes);
        return 1;
    }

    /* 事件帧 */
    if (len >= 6 && data[1] > 0x05 && data[2] == 0x99) {
        unsigned char code = data[5];
        if (code == 0x55) {
            printf("%s [HEARTBEAT] battery=%d%%\n", tag, data[3]);
            return 1;
        }
        if (code == 0x2E) {
            printf("%s [DPI ] level=%d\n", tag, data[4]);
            return 1;
        }
        if (code == 0x00) {
            printf("%s [BATTERY] %d%%\n", tag, data[3]);
            return 1;
        }
        if (code == 0x9A) {
            printf("%s [MEETING] created\n", tag);
            return 1;
        }
        if (code == 0x9B) {
            printf("%s [MEETING] destroyed\n", tag);
            return 1;
        }
        if (code == 0x35 && len >= 8) {
            printf("%s [KEY ] m(index=%d) %s\n", tag, data[6], key_name(data[7]));
            ++g_keys;
            return 1;
        }
        printf("%s [KEY ] %s (0x%02X)\n", tag, key_name(code), code);
        ++g_keys;
        return 1;
    }
    return 0;
}

/* ============================ SDK 回调（唯一派发线程，全局有序，不会并发进入） ============================ */
static void on_raw_frame(const char* id, const unsigned char* data, int len, int conn) {
    (void)id;
    ++g_frames;
    if (!data || len <= 0) return;
    if (parse_frame(data, len, conn) == 0)
        record_frame(data, len, conn);      /* 未识别帧完整留痕 */
}

static void on_connected(const char* id, int mode) {
    printf("\n>>> [CONNECTED] %s via %s <<<\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}
static void on_disconnected(const char* id, int mode) {
    printf("\n>>> [DISCONNECTED] %s via %s <<<\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}
static void on_message(const char* id, const char* json) {
    /* RAW 模式下 SDK 仍会上抛自身事件（如 pairingBroken 等），一并打印 */
    printf("[MSG ] device=%s %s\n", id, json);
}

/* ============================ 在线设备枚举 ============================ */
static void list_devices(void) {
    int n = um_get_device_count();
    printf("---- %d device(s) online ----\n", n);
    for (int i = 0; i < n; ++i) {
        const char* id = um_get_device_id(i);
        int mode = um_get_connection_mode(id);
        printf("  [%d] id=%s conn=%s\n", i, id ? id : "(null)",
               mode == UM_CONN_BLE ? "BLE" : (mode == UM_CONN_USB ? "USB" : "?"));
    }
}

/* ============================ 命令行辅助 ============================ */
static int read_rest_of_line(char* buf, int cap) {
    int i = 0, c;
    while ((c = getchar()) != EOF && c != '\n' && i < cap - 1) buf[i++] = (char)c;
    buf[i] = '\0';
    return (c == EOF) ? -1 : i;
}

static int parse_hex_line(const char* s, unsigned char* out, int cap) {
    int n = 0;
    while (*s && n < cap) {
        if (*s == ' ' || *s == ',') { ++s; continue; }
        if (!s[1]) break;
        char pair[3] = { s[0], s[1], 0 };
        char* end = NULL;
        unsigned long v = strtoul(pair, &end, 16);
        if (end != pair + 2) break;
        out[n++] = (unsigned char)v;
        s += 2;
    }
    return n;
}

/* ============================ main ============================ */
int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    printf("=== uMouseSdk demo4 (fully custom external hardware, RAW) ===\n");
    printf("version %s\n", um_sdk_version());
    printf("target: VID=0x%04X PID=0x%04X iface=%d report=0x%02X\n\n",
           HW_VID, HW_PID, HW_IFACE, HW_REPORT_ID);

    /* ---- 1. §1 替换默认规则标识为外部硬件（init 前完成）----
     * 传 NULL 即默认规则：本程序从头到尾只认你的硬件。
     * 要与声云生产设备共存时改用命名规则（见 demo3 的 rule_id 写法）。 */
    struct um_usb_config uconfig;
    memset(&uconfig, 0, sizeof(uconfig));
    uconfig.vid       = HW_VID;
    uconfig.pid       = HW_PID;
    uconfig.iface     = HW_IFACE;
    uconfig.report_id = HW_REPORT_ID;
    printf("[config] um_set_usb_config -> %d\n", um_set_usb_config(NULL, &uconfig));

    struct um_ble_config bconfig;
    memset(&bconfig, 0, sizeof(bconfig));
    bconfig.name           = HW_BLE_NAME;        /* 需要更复杂判断时改用
                                                  * um_register_ble_match_device 回调（见 demo2） */
    bconfig.mac_prefix     = HW_BLE_MAC_PREFIX;
    bconfig.service        = HW_BLE_SERVICE;     /* 服务 = 枚举过滤器 + 订阅对象 */
    bconfig.characteristic = HW_BLE_CHAR;        /* 特征 = notify 收 / write 发 */
    printf("[config] um_set_ble_config -> %d\n", um_set_ble_config(NULL, &bconfig));

    /* ---- 2. RAW 工作模式：SDK 不解析任何协议 ---- */
    printf("[config] um_set_work_mode(RAW) -> %d\n", um_set_work_mode(UM_MODE_RAW));

    /* ---- 3. 注册回调（握手/SN 类须 init 前注册） ---- */
    um_register_usb_build_probe(hw_build_probe);
    um_register_usb_validate_resp(hw_validate_resp);
    um_register_device_sn_probe(NULL, hw_sn_probe);   /* NULL = 默认规则 */
    um_register_raw_frame(on_raw_frame);
    um_register_connected(on_connected);
    um_register_disconnected(on_disconnected);
    um_register_message(on_message);
    printf("[config] callbacks registered\n");

    /* ---- 4. 启动（AUTO：USB 优先，静默超时转 BLE） ---- */
    printf("starting SDK...\n");
    um_sdk_init(1);

    printf("\n========================================\n");
    printf("Commands:\n");
    printf("  1/2/3 = channel pref USB / BLE / AUTO\n");
    printf("  l = list online devices (id = SN)\n");
    printf("  t = print distinct(unparsed) frame table\n");
    printf("  a = toggle print-every-frame mode\n");
    printf("  c = clear table\n");
    printf("  w [hex ..] = write bytes to device #0，如: w 0a 03 26 00\n");
    printf("               （无参数时发 SN 查询 0a 03 26 00 补零 10B）\n");
    printf("  q = quit\n");
    printf("========================================\n\n");

    int running = 1;
    while (running) {
        printf("> ");
        fflush(stdout);
        int c = getchar();
        switch (c) {
        case EOF:
            /* stdin 关闭（管道/GUI 拉起）时 getchar() 恒返回 EOF，
             * 不退出会 100% CPU 死循环（同 demo2 的 L2 教训） */
            printf("[exit] stdin closed\n");
            running = 0;
            break;
        case '1':
            printf("[cmd] um_set_channel_pref(USB) -> %d\n", um_set_channel_pref(UM_CHAN_USB));
            break;
        case '2':
            printf("[cmd] um_set_channel_pref(BLE) -> %d\n", um_set_channel_pref(UM_CHAN_BLE));
            break;
        case '3':
            printf("[cmd] um_set_channel_pref(AUTO) -> %d\n", um_set_channel_pref(UM_CHAN_AUTO));
            break;
        case 'l':
        case 'L':
            list_devices();
            break;
        case 't':
        case 'T':
            print_table();
            break;
        case 'a':
        case 'A':
            g_flood = !g_flood;
            printf("[cmd] print-every-frame %s\n", g_flood ? "ON" : "OFF");
            break;
        case 'c':
        case 'C':
            g_rec_n = 0;
            printf("[cmd] table cleared\n");
            break;
        case 'w':
        case 'W': {
            char line[256];
            if (read_rest_of_line(line, (int)sizeof(line)) < 0) {
                printf("[exit] stdin closed\n");
                running = 0;
                break;
            }
            const char* id = um_get_device_id(0);
            if (!id) {
                printf("[cmd] no device online\n");
                break;
            }
            unsigned char buf[64];
            int n = parse_hex_line(line, buf, (int)sizeof(buf));
            if (n <= 0) {
                /* 无参数：发 SN 查询（report_id + 03 26 00，补零 10B） */
                memset(buf, 0, 10);
                buf[0] = HW_REPORT_ID;
                buf[1] = 0x03;
                buf[2] = 0x26;
                buf[3] = 0x00;
                n = 10;
            }
            printf("[cmd] um_write_raw(device=%s, %d bytes): ", id, n);
            for (int i = 0; i < n; ++i) printf("%02x ", buf[i]);
            printf("-> %d\n", um_write_raw(id, buf, n));
            break;
        }
        case 'q':
        case 'Q':
            running = 0;
            break;
        case '\n':
        case '\r':
            break;
        default:
            break;
        }
    }

    um_sdk_close();
    printf("\ntotal: frames=%ld keys=%ld audio_pkts=%ld audio_bytes=%ld\n",
           g_frames, g_keys, g_audio_pkts, g_audio_bytes);
    print_table();
    return 0;
}
