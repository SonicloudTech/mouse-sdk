/*
 * demo.c —— uMouseSdk 最小使用示例（纯 C，验证对外 ABI）
 *
 * 注册 4 个回调 → um_sdk_init(1) 开日志 → 运行到按回车 → um_sdk_close。
 * 音频回调把 PCM 累加落盘为 out.pcm（16kHz/16bit/单声道，可用 Audacity 导入播放验证）。
 *
 * 构建：随 CMake 一起生成 umouse_demo.exe（与 uMouseSdk.dll 同目录，直接运行）。
 */
#include "umouse_sdk.h"

#include <stdio.h>

static FILE* g_pcm = NULL;
static long  g_pcm_bytes = 0;

static void on_connected(const char* id, int mode) {
    printf("[connected] %s via %s\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}
static void on_disconnected(const char* id, int mode) {
    printf("[disconnected] %s via %s\n", id, mode == UM_CONN_BLE ? "BLE" : "USB");
}
static void on_message(const char* id, const char* json) {
    printf("[message] %s %s\n", id, json);
}
static void on_audio(const char* id, const unsigned char* pcm, int len) {
    /* 示例为了简洁直接在回调里 fwrite+printf。注意：回调运行在 SDK 内部
     * 读/解码线程上，生产代码请勿在回调内做重量级 IO（应入队、由工作线程
     * 落盘），否则会阻塞 SDK 帧处理。 */
    (void)id;
    if (g_pcm && pcm && len > 0) {
        fwrite(pcm, 1, (size_t)len, g_pcm);
        g_pcm_bytes += len;
        printf("[audio len=]  %d\n", len);
    }
}

int main(void) {
    printf("uMouseSdk demo, version %s\n", um_sdk_version());

    g_pcm = fopen("out.pcm", "wb");
    if (!g_pcm) perror("fopen out.pcm");   /* 打不开仅少音频落盘，不阻断示例 */

    /* 注册须在 init 之前，避免错过早期事件。 */
    um_register_connected(on_connected);
    um_register_disconnected(on_disconnected);
    um_register_message(on_message);
    um_register_audio(on_audio);


    um_sdk_init(1);  /* debug=1 开内部日志 */

    printf("running... 按回车结束。\n");
    getchar();

    um_sdk_close();

    if (g_pcm) {
        fclose(g_pcm);
        printf("PCM 已写入 out.pcm，共 %ld 字节。\n", g_pcm_bytes);
    }
    return 0;
}
