/*
 * task_binding.c — 双核任务创建与绑核实现
 *
 * 绑核是这里唯一重要的事。其余都是模板样板。
 */
#include "task_binding.h"
#include <string.h>

/* 各任务入口，实现在各自模块内 */
extern void task_camera(void *arg);
extern void task_recognize(void *arg);
extern void task_ui(void *arg);
extern void task_cloud(void *arg);
extern void task_voice(void *arg);

int app_start(app_ctx_t *ctx)
{
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));

    ctx->q_frame   = xQueueCreate(Q_FRAME_DEPTH,   sizeof(void *));
    ctx->q_preview = xQueueCreate(Q_PREVIEW_DEPTH, sizeof(void *));
    ctx->q_voice   = xQueueCreate(Q_VOICE_DEPTH,   sizeof(int));

    ctx->sem_verified = xSemaphoreCreateBinary();
    ctx->mtx_flash    = xSemaphoreCreateMutex();

    if (!ctx->q_frame || !ctx->q_preview || !ctx->q_voice ||
        !ctx->sem_verified || !ctx->mtx_flash) {
        return -1;
    }

    /*
     * 绑核：core1 跑计算，core0 跑 IO。
     * 优先级：摄像头 > 推理 > 界面 > 云 > 语音。
     */
    xTaskCreatePinnedToCore(task_camera,    "cam",   4096, ctx, 6, &ctx->task_cam,       CORE_AI);
    xTaskCreatePinnedToCore(task_recognize, "rec",   8192, ctx, 5, &ctx->task_recognize, CORE_AI);
    xTaskCreatePinnedToCore(task_ui,        "ui",    8192, ctx, 4, &ctx->task_ui,        CORE_UI);
    xTaskCreatePinnedToCore(task_cloud,     "cloud", 4096, ctx, 3, &ctx->task_cloud,     CORE_UI);
    xTaskCreatePinnedToCore(task_voice,     "voice", 4096, ctx, 2, &ctx->task_voice,     CORE_UI);

    return 0;
}

void app_post_voice(app_ctx_t *ctx, int sound_id)
{
    /* 队列满时直接丢弃，不阻塞调用者 —— 语音提示不值得让业务流程等 */
    xQueueSend(ctx->q_voice, &sound_id, 0);
}

void app_show_preview(app_ctx_t *ctx, void *frame)
{
    xQueueSend(ctx->q_preview, &frame, 0);
}

int app_wait_verified(app_ctx_t *ctx, uint32_t timeout_ms)
{
    return xSemaphoreTake(ctx->sem_verified, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}
