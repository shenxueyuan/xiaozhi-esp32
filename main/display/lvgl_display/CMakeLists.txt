set(SOURCES
    lvgl_display.cc
    lvgl_theme.cc
)

idf_component_register(SRCS ${SOURCES}
                      INCLUDE_DIRS "." "lv_conf"
                      REQUIRES lvgl__lvgl)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_CONF_INCLUDE_SIMPLE=1)
