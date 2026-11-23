# LVGL 中文子集字库裁剪

## 问题

LVGL 默认字库里没有中文。如果用完整中文字库，16px + 24px 两套字库轻松超过 5MB，ESP32-S3 的 Flash 分区放不下。

## 解法

用 `lv_font_conv` 只生成界面实际用到的汉字。

## 步骤

### 1. 导出界面文案

把 UI 上出现的所有中文收集成一个字符串。注意要把动态内容也算进去（人名、书名可能出现的常用字）。

```bash
# 把界面文案整理成一行，去重
python3 -c "
import re
text = open('ui_strings.txt', encoding='utf-8').read()
chars = ''.join(sorted(set(re.findall(r'[\u4e00-\u9fa5]', text))))
print(chars, end='')
" > subset_chars.txt
```

### 2. 安装 lv_font_conv

```bash
npm i -g lv_font_conv
```

### 3. 生成字库

```bash
# 16px，1bpp（单色，体积最小，适合 OLED 类单色屏以外的彩色屏也需要 1bpp 才能压住体积）
lv_font_conv \
  --font SourceHanSansSC-Regular.otf \
  --symbols "$(cat subset_chars.txt)" \
  --size 16 \
  --bpp 1 \
  --format lvgl \
  --no-compress \
  -o lv_font_chinese_16.c

# 24px
lv_font_conv \
  --font SourceHanSansSC-Regular.otf \
  --symbols "$(cat subset_chars.txt)" \
  --size 24 \
  --bpp 1 \
  --format lvgl \
  -o lv_font_chinese_24.c
```

### 4. 关键参数说明

| 参数 | 取值 | 说明 |
|---|---|---|
| `--size` | 16 / 24 | 按界面设计稿的字号来 |
| `--bpp` | **1** | 每像素位数。1 = 单色，体积是 4bpp 的 1/4。中文笔画在这个尺寸下 1bpp 仍然清晰 |
| `--format` | `lvgl` | 生成 `.c` 文件，直接加进工程 |
| `--no-compress` | 加 | 省去解压开销；空间够就不压 |
| `--range` | 按需 | 只生成数字和字母时用 `--range 0x20-0x7F` |

### 5. 声明字体

```c
LV_FONT_DECLARE(lv_font_chinese_16);
LV_FONT_DECLARE(lv_font_chinese_24);

lv_obj_set_style_text_font(label, &lv_font_chinese_16, 0);
```

## 效果

| 方案 | 体积 |
|---|---|
| 完整中文字库 16px + 24px | 5–8 MB |
| 子集字库（约 300 字）1bpp | 30–60 KB |

省出 2MB 以上 Flash，用在了固件双备份和 OTA 分区上。

## 注意

字库是按文案静态生成的，**运行时出现字库外的字会显示成空白**。两个应对办法：

1. 用户输入类内容（书名、人名）走云端渲染成图片下发
2. 提前把常用字（约 3500 一级字）也打进去，体积约 400KB，仍然可接受

本项目用的是方案 1 + 动态内容共用固定词表（如"图书""借阅""归还"）。
