/*
 * demo3.c —— 多设备接入示例（纯 C）
 *
 * 场景：两类设备同时在线——
 *   规则 A（默认规则，不配置即生产设备）：FULL 完整协议解析。
 *   规则 B（rule_id="ext"，RAW 自定义协议）：注册独立识别标识 + 握手回调 +
 *           SN 提取回调，SDK 透传原始帧，本 demo 自行解析。
 *
 * 演示要点：
 *   1. device_id = SN（FULL 设备由 SDK 内部识别；RAW 设备经
 *      um_register_device_sn_probe 提取）。同一设备的 USB/BLE 切换不改变身份。
 *   2. um_get_device_count / um_get_device_id / um_get_connection_mode 枚举在线设备。
 *   3. 会话类命令（um_write_raw 等）多设备下须显式指定 device_id；
 *      单设备在线时 NULL 兼容旧用法。
 *   4. 所有回调在 SDK 唯一事件派发线程触发，全局有序、不会并发进入。
 *
 * 规则 B 的标识按你的实际设备修改下方 EXT_* 宏（示例值为自定义协议设备）。
 *
 * 构建随 CMake 生成 umouse_demo3.exe（与 uMouseSdk.dll 同目录，直接运行）。
 */
#include "umouse_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================ 规则 B：自定义协议设备标识（按实际设备修改） ============================ */
#define EXT_RULE_ID    "ext"
#define EXT_VID        0x2480
#define EXT_PID        0x6698
#define EXT_IFACE      (-1)      /* <0 不限接口，取第一个匹配 VID/PID 的 */
#define EXT_REPORT_ID  0x00

#define EXT_BLE_NAME        NULL     /* NULL=不按名称匹配 */
#define EXT_BLE_MAC_PREFIX  NULL     /* NULL=不按 MAC 前缀匹配（配合 match 回调使用） */
#define EXT_BLE_SERVICE     NULL     /* NULL=保留默认 service */
#define EXT_BLE_CHAR        NULL     /* NULL=保留默认 characteristic */

/* 自定义协议帧：fe [seq] [cmd] [param] 00 00 00 [payload...] [crc32 x4]
 * 初始化查询（握手）：cmd=0x06 param=0x01，末 4 字节为抓包固定 CRC。 */
static const unsigned char EXT_PROBE[12] = {
    0xfe, 0x00, 0x06, 0x01, 0x00, 0x00, 0x00, 0x08, 0x9e, 0x0b, 0xa4, 0xd8
};

/* ============================ 规则 B：握手回调（ex 版，带 rule_id） ============================ */
static int ext_build_probe(const char* rule_id, unsigned char* out, int max_len) {
    (void)rule_id;
    if (max_len < (int)sizeof(EXT_PROBE)) return 0;
    memcpy(out, EXT_PROBE, sizeof(EXT_PROBE));
    return (int)sizeof(EXT_PROBE);
}

static int ext_validate_resp(const char* rule_id, const unsigned char* buf, int len) {
    (void)rule_id;
    /* 帧头 0xfe + cmd 高位为 1（0x81 响应 / 0x47 事件 / 0xa0 音频）= 合法设备帧 */
    if (len < 4) return 0;
    return buf[0] == 0xfe && (buf[2] & 0x80);
}

static int ext_ble_match(const char* rule_id, const char* name, const char* address) {
    /* 按你的设备命名/MA 规则改写；示例：接受全部（演示用，生产勿照抄） */
    (void)rule_id;
    printf("[ext-match] name=%s addr=%s\n", name ? name : "(null)", address ? address : "(null)");
    return 1;
}

/* ============================ 规则 B：SN 提取回调 ============================
 * 自定义协议的 SN 藏在异步事件帧（cmd=0x47 param=0x06，payload 为 ASCII）。
 * 识别窗口内 SDK 把每帧交给本回调，提取成功返回 1。
 * 契约：SN 必须每台唯一（是型号号则不要返回——两台会被并成一台）。 */
static int ext_sn_probe(const char* rule_id, const unsigned char* data, int len, char* out_sn, int out_cap) {
    (void)rule_id;
    if (len < 9) return 0;
    if (data[0] != 0xfe || data[2] != 0x47 || data[3] != 0x06) return 0;
    int sn_len = len - 12;  /* payload = 总长 - 帧头 8 字节 - CRC32 4 字节 */
    if (sn_len <= 0 || sn_len >= out_cap) return 0;
    memcpy(out_sn, data + 8, (size_t)sn_len);
    out_sn[sn_len] = '\0';
    return 1;
}

/* ============================ 事件回调（唯一派发线程，全局有序） ============================ */
static void on_connected(const char* id, int mode) {
    printf("\n>>> [CONNECTED] device=%s via %s <<<\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}

static void on_disconnected(const char* id, int mode) {
    printf("\n>>> [DISCONNECTED] device=%s via %s <<<\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}

static void on_message(const char* id, const char* json) {
    printf("[MSG ] device=%s %s\n", id, json);
}

static void on_audio(const char* id, const unsigned char* pcm, int len) {
    (void)id; (void)pcm; (void)len;
    /* 音频量大，示例只计数不打内容 */
    static long n = 0;
    if (++n % 100 == 0)
        printf("[AUD ] device=%s +%d pcm (total %ld)\n", id, len, n);
}

static void on_raw_frame(const char* id, const unsigned char* data, int len, int conn) {
    printf("[RAW ] device=%s via %s len=%d  ", id, conn == UM_CONN_BLE ? "BLE" : "USB", len);
    for (int i = 0; i < len && i < 16; ++i) printf("%02x ", data[i]);
    if (len > 16) printf("...");
    printf("\n");
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

/* ============================ main ============================ */
int main(void) {
    printf("=== uMouseSdk demo3 (multi-device) ===\n");
    printf("version %s\n\n", um_sdk_version());

    /* ---- 1. 规则 A：默认规则（生产设备 FULL），无需任何配置 ---- */

    /* ---- 2. 规则 B：注册自定义设备识别标识（init 前完成） ---- */
    struct um_usb_config uconfig;
    memset(&uconfig, 0, sizeof(uconfig));
    uconfig.vid       = EXT_VID;
    uconfig.pid       = EXT_PID;
    uconfig.iface     = EXT_IFACE;
    uconfig.report_id = EXT_REPORT_ID;
    printf("[config] um_set_usb_config(rule=%s) -> %d\n", EXT_RULE_ID, um_set_usb_config(EXT_RULE_ID, &uconfig));

    struct um_ble_config bconfig;
    memset(&bconfig, 0, sizeof(bconfig));
    bconfig.name           = EXT_BLE_NAME;
    bconfig.mac_prefix     = EXT_BLE_MAC_PREFIX;
    bconfig.service        = EXT_BLE_SERVICE;
    bconfig.characteristic = EXT_BLE_CHAR;
    printf("[config] um_set_ble_config(rule=%s) -> %d\n", EXT_RULE_ID, um_set_ble_config(EXT_RULE_ID, &bconfig));

    /* 规则 B：RAW 工作模式（非默认规则默认即 RAW，此处显式声明便于阅读） */
    printf("[config] um_set_work_mode_for(%s, RAW) -> %d\n", EXT_RULE_ID,
           um_set_work_mode_for(EXT_RULE_ID, UM_MODE_RAW));

    /* ---- 3. 注册回调（事件回调可运行期换；握手/SN 类须 init 前） ---- */
    um_register_usb_build_probe_for(EXT_RULE_ID, ext_build_probe);
    um_register_usb_validate_resp_for(EXT_RULE_ID, ext_validate_resp);
    um_register_ble_match_device_for(EXT_RULE_ID, ext_ble_match);
    um_register_device_sn_probe(EXT_RULE_ID, ext_sn_probe);
    um_register_connected(on_connected);
    um_register_disconnected(on_disconnected);
    um_register_message(on_message);
    um_register_audio(on_audio);
    um_register_raw_frame(on_raw_frame);
    printf("[config] callbacks registered\n");

    /* ---- 4. 启动：两类规则各自探测，设备识别完成后上报 connected ---- */
    printf("starting SDK...\n");
    um_sdk_init(1);

    printf("\n========================================\n");
    printf("Commands:\n");
    printf("  l = list online devices (count/id/conn)\n");
    printf("  w = write raw to device #0 (must specify device when >1 online)\n");
    printf("  q = quit\n");
    printf("========================================\n\n");

    int running = 1;
    while (running) {
        printf("> ");
        fflush(stdout);
        int c = getchar();
        switch (c) {
        case EOF:
            printf("[exit] stdin closed\n");
            running = 0;
            break;
        case 'l':
        case 'L':
            list_devices();
            break;
        case 'w':
        case 'W': {
            /* 多设备下写指令必须显式指定 device_id（NULL 仅在唯一在线时有效） */
            const char* id = um_get_device_id(0);
            if (!id) {
                printf("[cmd] no device online\n");
                break;
            }
            unsigned char cmd[10] = {0x0A, 0x03, 0x26, 0x00};
            printf("[cmd] um_write_raw(device=%s) -> %d\n", id, um_write_raw(id, cmd, 10));
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
    return 0;
}
