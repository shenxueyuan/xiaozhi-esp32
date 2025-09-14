#ifndef LV_CONF_H
#define LV_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* 包含LVGL默认配置 */
#include "lvgl__lvgl/src/lv_conf_internal.h"

/* 启用快照功能 */
#define LV_USE_SNAPSHOT 1

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_CONF_H */
