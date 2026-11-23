/*
 * task_binding.h — ESP32-S3 双核任务创建与绑核模板
 *
 * 核心思想：计算密集任务绑 core1，IO 等待型任务绑 core0。
 * 这样 AI 推理跑满一个核时，界面渲染仍能稳定在另一个核上。
 */
#ifndef TASK_BINDING_H
#define TASK_BINDING_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#define CORE_AI   1     /* 摄像头 + 推理 */
#define CORE_UI   0     /* 界面 + 云 + 语音 */

/* 队列深度刻意设小，满了就丢旧帧，避免延迟累积和内存耗尽 */
#define Q_FRAME_DEPTH    2
#define Q_PREVIEW_DEPTH  2
#define Q_VOICE_DEPTH    8

typedef struct {
    TaskHandle_t  task_cam;
    TaskHandle_t  task_recognize;
    TaskHandle_t  task_ui;
    TaskHandle_t  task_cloud;
    TaskHandle_t  task_voice;

    QueueHandle_t q_frame;      /* 摄像头 -> 识别（帧指针） */
    QueueHandle_t q_preview;    /* 识别   -> 界面（抓拍预览） */
    QueueHandle_t q_voice;      /* 任意   -> 语音（播放请求） */

    SemaphoreHandle_t sem_verified;   /* 核验通过，放行借阅 */
    SemaphoreHandle_t mtx_flash;      /* 保护人脸库 Flash 读写 */
} app_ctx_t;

/* 创建全部任务与同步对象，返回 0 成功 */
int app_start(app_ctx_t *ctx);

/* 供其他任务调用的轻量接口，不暴露内部实现 */
void app_post_voice(app_ctx_t *ctx, int sound_id);
void app_show_preview(app_ctx_t *ctx, void *frame);
int  app_wait_verified(app_ctx_t *ctx, uint32_t timeout_ms);

#endif /* TASK_BINDING_H */
