# Desktop SparkBot 表情文件设置指南

## 🎯 问题说明

Desktop SparkBot 需要6个全屏表情GIF文件才能完成编译。当前链接错误显示缺少以下GIF资源：

```
undefined reference to `fullscreen_neutral'
undefined reference to `fullscreen_happy'
undefined reference to `fullscreen_sad'
undefined reference to `fullscreen_angry'
undefined reference to `fullscreen_surprised'
undefined reference to `fullscreen_thinking'
```

## 📁 表情文件要求

### 文件规格
- **尺寸**: 240x240 像素 (全屏显示)
- **格式**: GIF 动画
- **颜色**: 支持彩色，建议使用明亮色彩
- **帧率**: 15-30 FPS (系统会自动调整)
- **文件大小**: 建议每个文件 < 500KB

### 需要的6个表情
1. **fullscreen_neutral.gif** - 中性/平静表情
2. **fullscreen_happy.gif** - 开心/高兴表情
3. **fullscreen_sad.gif** - 悲伤表情
4. **fullscreen_angry.gif** - 愤怒表情
5. **fullscreen_surprised.gif** - 惊讶表情
6. **fullscreen_thinking.gif** - 思考/困惑表情

## 🛠️ 操作步骤

### 步骤1: 准备GIF文件
将6个GIF文件准备好，命名为上述要求的文件名。

### 步骤2: 转换为LVGL图像资源

#### 方法A: 使用在线转换工具
1. 访问 LVGL 在线图像转换器: https://lvgl.io/tools/imageconverter
2. 上传每个GIF文件
3. 设置参数:
   - **Color format**: CF_TRUE_COLOR
   - **Output format**: C array
   - **Name**: 使用对应的资源名 (如 fullscreen_neutral)
4. 下载生成的 `.c` 文件

#### 方法B: 使用LVGL图像转换脚本
```bash
# 如果你有LVGL开发环境
python3 lv_img_conv.py fullscreen_neutral.gif --cf CF_TRUE_COLOR --format C
python3 lv_img_conv.py fullscreen_happy.gif --cf CF_TRUE_COLOR --format C
python3 lv_img_conv.py fullscreen_sad.gif --cf CF_TRUE_COLOR --format C
python3 lv_img_conv.py fullscreen_angry.gif --cf CF_TRUE_COLOR --format C
python3 lv_img_conv.py fullscreen_surprised.gif --cf CF_TRUE_COLOR --format C
python3 lv_img_conv.py fullscreen_thinking.gif --cf CF_TRUE_COLOR --format C
```

### 步骤3: 创建表情资源组件

#### 3.1 创建组件目录
```bash
mkdir -p managed_components/desktop_sparkbot_gifs/src
mkdir -p managed_components/desktop_sparkbot_gifs/include
```

#### 3.2 放置转换后的C文件
将转换后的6个 `.c` 文件放入:
```
managed_components/desktop_sparkbot_gifs/src/
├── fullscreen_neutral.c
├── fullscreen_happy.c
├── fullscreen_sad.c
├── fullscreen_angry.c
├── fullscreen_surprised.c
└── fullscreen_thinking.c
```

#### 3.3 创建头文件
创建 `managed_components/desktop_sparkbot_gifs/include/desktop_sparkbot_gifs.h`:
```cpp
#pragma once

#include <libs/gif/lv_gif.h>

#ifdef __cplusplus
extern "C" {
#endif

// Desktop SparkBot 全屏表情GIF声明
LV_IMAGE_DECLARE(fullscreen_neutral);
LV_IMAGE_DECLARE(fullscreen_happy);
LV_IMAGE_DECLARE(fullscreen_sad);
LV_IMAGE_DECLARE(fullscreen_angry);
LV_IMAGE_DECLARE(fullscreen_surprised);
LV_IMAGE_DECLARE(fullscreen_thinking);

#ifdef __cplusplus
}
#endif
```

#### 3.4 创建CMakeLists.txt
创建 `managed_components/desktop_sparkbot_gifs/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS
        "src/fullscreen_neutral.c"
        "src/fullscreen_happy.c"
        "src/fullscreen_sad.c"
        "src/fullscreen_angry.c"
        "src/fullscreen_surprised.c"
        "src/fullscreen_thinking.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        lvgl__lvgl
)
```

#### 3.5 创建组件配置
创建 `managed_components/desktop_sparkbot_gifs/idf_component.yml`:
```yaml
version: "1.0.0"
description: Desktop SparkBot全屏表情GIF资源
url: https://github.com/your-repo/desktop-sparkbot-gifs
dependencies:
  lvgl/lvgl: "^9.2.0"
```

### 步骤4: 更新Desktop SparkBot代码

修改 `main/boards/desktop-sparkbot/fullscreen_emoji_display.h`，将：
```cpp
// 声明GIF资源（需要准备全屏尺寸的GIF）
LV_IMAGE_DECLARE(fullscreen_neutral);
LV_IMAGE_DECLARE(fullscreen_happy);
// ...
```

替换为：
```cpp
#include <desktop_sparkbot_gifs.h>
```

### 步骤5: 重新编译
```bash
idf.py fullclean
idf.py reconfigure
idf.py build
```

## 🎨 临时解决方案

如果暂时没有合适的GIF文件，可以使用现有的Otto表情作为占位符：

### 修改 fullscreen_emoji_display.h
```cpp
#pragma once

#include "display/lcd_display.h"
#include <libs/gif/lv_gif.h>
#include <otto_emoji_gif.h>  // 使用现有的Otto表情

class FullscreenEmojiDisplay : public SpiLcdDisplay {
    // ... 其他代码保持不变
};
```

### 修改 fullscreen_emoji_display.cc 中的映射表
```cpp
const FullscreenEmojiDisplay::EmotionGifMap FullscreenEmojiDisplay::emotion_gif_maps_[] = {
    // 临时使用Otto表情，后续替换为全屏版本
    {"neutral", &staticstate, 15},
    {"relaxed", &staticstate, 10},
    {"sleepy", &staticstate, 8},
    {"happy", &happy, 25},
    {"laughing", &happy, 30},
    {"sad", &sad, 15},
    {"crying", &sad, 20},
    {"angry", &anger, 30},
    {"surprised", &scare, 25},
    {"shocked", &scare, 30},
    {"thinking", &buxue, 10},
    {"confused", &buxue, 12},
    {nullptr, nullptr, 0}
};
```

这样可以先让项目编译通过，后续再替换为全屏GIF。

## 📞 需要帮助？

如果您需要：
1. **GIF文件制作**: 可以使用现有的小尺寸表情放大到240x240
2. **格式转换**: 我可以提供详细的转换步骤
3. **组件集成**: 我可以帮助完成组件的创建和集成

请告诉我您希望采用哪种方案！

---
*创建时间: 2024年12月*
*适用版本: xiaozhi-esp32 v1.8.8 + Desktop SparkBot*
