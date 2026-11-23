# Libra 系统架构

## 分层

```
┌───────────────────────────────────────────────┐
│  应用层  借阅流程 / 库存管理 / 人脸库更新       │
├───────────────────────────────────────────────┤
│  交互层  LVGL 4 界面 / 语音播报                 │
├───────────────────────────────────────────────┤
│  识别层  ESP-WHO 检测 + 1:N 比对               │
├───────────────────────────────────────────────┤
│  感知层  摄像头抓拍 / 光敏 / 温湿度             │
├───────────────────────────────────────────────┤
│  支撑层  FreeRTOS / ESP-IDF / NVS / MQTT       │
└───────────────────────────────────────────────┘
```

## 任务与绑核

ESP32-S3 是双核 Xtensa LX7（240MHz）。AI 推理是 CPU 密集操作，如果和 UI 渲染同核，界面必然掉帧。

```c
xTaskCreatePinnedToCore(task_camera,   "cam",   4096, NULL, 6, NULL, 1);
xTaskCreatePinnedToCore(task_recognize,"rec",   8192, NULL, 5, NULL, 1);
xTaskCreatePinnedToCore(task_ui,       "ui",    8192, NULL, 4, NULL, 0);
xTaskCreatePinnedToCore(task_cloud,    "cloud", 4096, NULL, 3, NULL, 0);
xTaskCreatePinnedToCore(task_voice,    "voice", 4096, NULL, 2, NULL, 0);
```

| 核 | 任务 | 理由 |
|---|---|---|
| core1 | 摄像头 + 推理 | 都是计算密集，互相让位不影响用户感知 |
| core0 | UI + 云 + 语音 | 都是 IO 等待型，需要低延迟响应触摸 |

结果：推理进行时 UI 帧率仍稳定 30fps。

## 任务间通信

| 通道 | 类型 | 深度 | 生产者 → 消费者 |
|---|---|---|---|
| `q_frame` | 队列 | 2 | TaskCam → TaskRec（传帧指针，深度 2 防积压） |
| `q_preview` | 队列 | 2 | TaskRec → TaskUI（抓拍预览） |
| `q_voice` | 队列 | 8 | 各任务 → TaskVoice（事件触发的播放请求） |
| `sem_verified` | 二值信号量 | — | TaskRec → 借阅流程 |
| `mtx_flash` | 互斥量 | — | 保护人脸库读写 |

队列深度刻意设小：`q_frame` 深度 2 意味着识别跟不上时，摄像头任务会被阻塞并丢弃旧帧，而不是让帧在队列里堆积导致内存耗尽和延迟累积。

## 一次完整借阅

```
1. 用户在 UI 选择"借书"
2. TaskUI 置 state = WAIT_FACE，启动摄像头
3. TaskCam 每 200ms 抓一帧，投 q_frame
4. TaskRec 取帧 -> human_face_detect -> 特征提取 -> 1:N 比对
5. 命中：
   a. xSemaphoreGive(sem_verified)
   b. q_voice 投递"验证通过"提示音
   c. 库存 -1，写 NVS
   d. q_preview 投抓拍帧到 UI 显示
   e. TaskCloud 上报借阅记录
6. 未命中：UI 提示重试，回到步骤 3
```

`sema` 而不是 `queue` 的原因：借阅流程只关心"通过/未通过"这一件事，二值信号量语义最贴切，且开销最小。

## 内存

ESP32-S3 8MB PSRAM + 512KB SRAM。

| 区域 | 用途 |
|---|---|
| SRAM | 任务栈、队列、LVGL 对象池 |
| PSRAM | 摄像头帧缓冲（2 帧 double buffering）、字库、PCM 资源 |

LVGL 的 `lv_conf.h` 里 `LV_MEM_SIZE` 设在 SRAM，避免 PSRAM 访问延迟影响渲染；帧缓冲和音频资源放 PSRAM，因为它们是大块顺序访问，延迟不敏感。
