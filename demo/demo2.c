/*
 * demo2.c —— 声云鼠标 RAW 工作模式示例（纯 C，USB + BLE）
 *
 * 验证 SDK「RAW 模式」完整自定义链路：SDK 不做协议解析，原始帧透传给上位机。
 * 本 demo 在 raw_frame 回调里自行解析声云鼠标协议（与 SDK FULL 模式的 ParseRawKey 同源）。
 *
 * 支持交互式通道切换：
 *   运行后输入命令切换 USB / BLE / AUTO，观察不同通道的原始帧。
 *   USB 优先（AUTO 模式）；BLE 需鼠标已被系统用蓝牙连上。
 *
 * 构建随 CMake 生成 umouse_demo2.exe（与 uMouseSdk.dll 同目录，直接运行）。
 */
#include "umouse_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================ 声云设备标识（生产默认值） ============================ */
#define DEMO_VID       0xABC9
#define DEMO_PID       0xCA89
#define DEMO_IFACE     1
#define DEMO_REPORT_ID 0x0A

#define BLE_NAME        "uMouse"
#define BLE_MAC_PREFIX  "C0:88"
#define BLE_SERVICE     "0000fff0-0000-1000-8000-00805f9b34fb"
#define BLE_CHAR        "0783b03e-8535-b5a0-7140-a304d2495cb8"

/* ============================ 音频落盘 ============================ */
static FILE* g_audio = NULL;
static long  g_audio_bytes = 0;
static long  g_frame_count = 0;

/* ============================ 事件码 → 名称 ============================ */
static const char* key_name(unsigned char code) {
    switch (code) {
        case 0x21: return "speech down";
        case 0x22: return "speech up";
        case 0x2A: return "translation down";
        case 0x2B: return "translation up";
        case 0x23: return "ai down";
        case 0x24: return "ai up";
        case 0x31: return "ai click";
        case 0x25: return "capture click";
        case 0x27: return "m key down";
        case 0x29: return "m key hold";
        case 0x28: return "m key up";
        default:   return "unknown";
    }
}

/* ============================ hex 打印 ============================ */
static void print_hex(const unsigned char* d, int len) {
    for (int i = 0; i < len && i < 32; ++i) printf("%02x ", d[i]);
    if (len > 32) printf("...");
    printf("\n");
}

/* ============================ USB 握手回调 ============================
 * 声云设备 MI_01 是 vendor-defined 接口，hid_write 能成功。
 * 注册 build_probe/validate_resp 验证「自定义握手」整条链路也能走通。
 */
static int my_build_probe(unsigned char* out, int max_len) {
    if (max_len < 10) return 0;
    out[0] = DEMO_REPORT_ID;
    out[1] = 0x03;
    out[2] = 0xA0;
    out[3] = 0x00;
    return 10;
}

static int my_validate_resp(const unsigned char* buf, int len) {
    if (len < 6) return 0;
    if (buf[0] != DEMO_REPORT_ID) return 0;
    if (buf[1] <= 0x05) return 0;
    if (buf[2] != 0x99) return 0;
    if (buf[5] != 0x00) return 0;
    return 1;
}

/* ============================ BLE 目标设备识别回调 ============================
 * BLE 枚举到系统已连接设备时调用，判断是否为声云鼠标。
 * 声云鼠标 BLE 名称 "uMouse"，MAC 前缀按位掩码匹配（首字节 & 0xC0==0xC0 且次字节==0x88）。
 */
static int my_ble_match_device(const char* name, const char* address) {
    /* 名称精确匹配 */
    if (name && strcmp(name, BLE_NAME) == 0)
        return 1;
    /* MAC 前缀匹配：解析前两字节做位掩码判断 */
    if (address && strlen(address) >= 5) {
        char hex[3] = {0};
        int first, second;
        hex[0] = address[0]; hex[1] = address[1];
        first = (int)strtol(hex, NULL, 16);
        hex[0] = address[3]; hex[1] = address[4];
        second = (int)strtol(hex, NULL, 16);
        if (((first & 0xC0) == 0xC0) && (second == 0x88))
            return 1;
    }
    return 0;
}

/* ============================ 协议帧解析（USB / BLE 统一入口） ============================
 * 事件码统一取 data[5]（USB 与 BLE 一致；如真机 BLE 事件码错位再按下述区分：
 * USB 帧 [0x0A report_id] [类型] [0x99] [...] [...] [事件码]，
 * BLE 帧少一字节 report_id 前缀时事件码为 data[4]）
 */
static void parse_frame(const unsigned char* data, int len, int conn) {
    if (!data || len < 3) return;

    const char* tag = (conn == UM_CONN_BLE) ? "[BLE]" : "[USB]";

    /* —— SN 上报：USB data[0]==data[1]==0x0A；BLE data[0]==0x0A, data[1]==0x0A 也成立 —— */
    /* USB: data[0]==0x0A && data[1]==0x0A; BLE: 同样（BLE 的 data[0] 也是 0x0A 协议头） */
    if (len >= 10 && data[0] == DEMO_REPORT_ID && data[1] == DEMO_REPORT_ID) {
        char sn[17] = {0};
        static const char hex[] = "0123456789ABCDEF";
        for (int i = 0; i < 8; ++i) {
            sn[i * 2]     = hex[(data[2 + i] >> 4) & 0x0F];
            sn[i * 2 + 1] = hex[data[2 + i] & 0x0F];
        }
        printf("%s [SN] %s\n", tag, sn);
        return;
    }

    /* —— 音频帧：USB data[1]==0x9C；BLE data[1]==0x9C —— */
    if (len >= 2 && data[1] == 0x9C) {
        if (g_audio && len > 2) {
            fwrite(data + 2, 1, (size_t)(len - 2), g_audio);
            g_audio_bytes += (len - 2);
        }
        if (g_frame_count % 200 == 0)
            printf("%s [AUDIO] %ld frames, %ld bytes\n", tag, g_frame_count, g_audio_bytes);
        return;
    }

    /* —— 事件帧 —— */
    /* USB: data[1]>0x05 && data[2]==0x99, 事件码=data[5]
     * BLE: 事件码同样取 data[5]（通道层已按协议对齐偏移，与文件头说明的
     * "BLE=data[4]" 不同——以 data[5] 统一处理；若真机 BLE 事件码错位再区分） */
    if (data[1] > 0x05 && len >= 6 && data[2] == 0x99) {
        unsigned char code = data[5];   /* 事件码 */

        if (code == 0x55) {
            printf("%s [HEARTBEAT] battery=%d%%\n", tag, data[3]);
            return;
        }
        if (code == 0x2E) {
            printf("%s [DPI] level=%d\n", tag, data[4]);
            return;
        }
        if (code == 0x00) {
            printf("%s [BATTERY] %d%%\n", tag, data[3]);
            return;
        }
        if (code == 0x9A) {
            printf("%s [MEETING] created\n", tag);
            return;
        }
        if (code == 0x9B) {
            printf("%s [MEETING] destroyed\n", tag);
            return;
        }
        if (code == 0x35 && len >= 8) {
            printf("%s [KEY] m(index=%d) %s\n", tag, data[6], key_name(data[7]));
            return;
        }
        printf("%s [KEY] %s\n", tag, key_name(code));
        return;
    }

    /* —— 其它帧：打印 hex —— */
    printf("%s [RAW] len=%d  ", tag, len);
    print_hex(data, len);
}

/* ============================ 原始帧回调 ============================ */
static void on_raw_frame(const char* id, const unsigned char* data, int len, int conn) {
    (void)id;
    g_frame_count++;
    parse_frame(data, len, conn);
}

/* ============================ 连接状态回调 ============================ */
static void on_connected(const char* id, int mode) {
    printf("\n>>> [CONNECTED] %s via %s <<<\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}
static void on_disconnected(const char* id, int mode) {
    printf("\n>>> [DISCONNECTED] %s via %s <<<\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}

/* ============================ main ============================ */
int main(void) {
    printf("=== uMouseSdk demo2 (RAW mode, USB + BLE) ===\n");
    printf("version %s\n\n", um_sdk_version());

    g_audio = fopen("demo2_audio.raw", "wb");

    /* ---- 1. 设置 USB 标识 ---- */
    struct um_usb_config uconfig;
    memset(&uconfig, 0, sizeof(uconfig));
    uconfig.vid       = DEMO_VID;
    uconfig.pid       = DEMO_PID;
    uconfig.iface     = DEMO_IFACE;
    uconfig.report_id = DEMO_REPORT_ID;
    printf("[config] um_set_usb_config -> %d\n", um_set_usb_config(NULL, &uconfig));

    /* ---- 2. 设置 BLE 标识 ---- */
    struct um_ble_config bconfig;
    memset(&bconfig, 0, sizeof(bconfig));
    bconfig.name           = BLE_NAME;
    bconfig.mac_prefix     = BLE_MAC_PREFIX;
    bconfig.service        = BLE_SERVICE;
    bconfig.characteristic = BLE_CHAR;
    printf("[config] um_set_ble_config -> %d\n", um_set_ble_config(NULL, &bconfig));

    /* ---- 3. 设置 RAW 工作模式 ---- */
    printf("[config] um_set_work_mode(RAW) -> %d\n", um_set_work_mode(UM_MODE_RAW));

    /* ---- 4. 注册回调 ---- */
    um_register_usb_build_probe(my_build_probe);
    um_register_usb_validate_resp(my_validate_resp);
    /* BLE 目标设备识别：自定义判断逻辑替代默认 name+mac 匹配 */
    um_register_ble_match_device(my_ble_match_device);
    um_register_raw_frame(on_raw_frame);
    um_register_connected(on_connected);
    um_register_disconnected(on_disconnected);
    printf("[config] callbacks registered\n");

    /* ---- 5. 启动 SDK（默认 AUTO：USB 优先，USB 静默超时转 BLE）---- */
    printf("starting SDK...\n");
    um_sdk_init(1);

    /* ---- 6. 交互式通道切换 ---- */
    printf("\n========================================\n");
    printf("Commands:\n");
    printf("  1 = USB only    (强制只走 2.4G 接收器)\n");
    printf("  2 = BLE only    (强制只走蓝牙，跳过接收器)\n");
    printf("  3 = AUTO        (USB 优先，失败转 BLE)\n");
    printf("  w = send test write (下发 0x0A 0x03 0x26 0x00 查 SN)\n");
    printf("  q = quit\n");
    printf("========================================\n\n");

    int running = 1;
    while (running) {
        printf("> ");
        fflush(stdout);
        int c = getchar();
        switch (c) {
        case EOF:
            /* stdin 关闭（管道 / 无控制台 / GUI 拉起）时 getchar() 恒返回 EOF，
             * 不显式退出会落 default 分支 100% CPU 死循环（第六轮自省，同 L2） */
            printf("[exit] stdin closed\n");
            running = 0;
            break;
        case '1':
            printf("[cmd] switching to USB only...\n");
            printf("[cmd] um_set_channel_pref(USB) -> %d\n", um_set_channel_pref(UM_CHAN_USB));
            break;
        case '2':
            printf("[cmd] switching to BLE only...\n");
            printf("[cmd] um_set_channel_pref(BLE) -> %d\n", um_set_channel_pref(UM_CHAN_BLE));
            break;
        case '3':
            printf("[cmd] switching to AUTO...\n");
            printf("[cmd] um_set_channel_pref(AUTO) -> %d\n", um_set_channel_pref(UM_CHAN_AUTO));
            break;
        case 'w':
        case 'W': {
            /* 下发查 SN 命令：{report_id, 0x03, 0x26, 0x00} */
            unsigned char cmd[10] = {DEMO_REPORT_ID, 0x03, 0x26, 0x00};
            printf("[cmd] um_write_raw -> %d\n", um_write_raw(NULL, cmd, 10));
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

    if (g_audio) {
        fclose(g_audio);
        printf("\naudio: demo2_audio.raw, %ld bytes, %ld frames total\n", g_audio_bytes, g_frame_count);
    }
    return 0;
}
