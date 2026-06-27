set(LVGL_ROOT ${CMAKE_CURRENT_LIST_DIR}/lvgl)

file(GLOB_RECURSE LVGL_CORE_SOURCES CONFIGURE_DEPENDS
    ${LVGL_ROOT}/src/core/*.c
    ${LVGL_ROOT}/src/display/*.c
    ${LVGL_ROOT}/src/draw/*.c
    ${LVGL_ROOT}/src/font/*.c
    ${LVGL_ROOT}/src/indev/*.c
    ${LVGL_ROOT}/src/layouts/*.c
    ${LVGL_ROOT}/src/libs/bin_decoder/*.c
    ${LVGL_ROOT}/src/misc/*.c
    ${LVGL_ROOT}/src/osal/*.c
    ${LVGL_ROOT}/src/others/observer/*.c
    ${LVGL_ROOT}/src/others/sysmon/*.c
    ${LVGL_ROOT}/src/stdlib/*.c
    ${LVGL_ROOT}/src/themes/*.c
    ${LVGL_ROOT}/src/tick/*.c
    ${LVGL_ROOT}/src/widgets/*.c
    ${LVGL_ROOT}/src/lv_*.c
)

list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/draw/dma2d/.*")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/draw/nema_gfx/.*")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/draw/nxp/.*")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/draw/opengles/.*")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/draw/renesas/.*")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/draw/sdl/.*")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/draw/vg_lite/.*")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/osal/lv_cmsis_rtos2\\.c")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/osal/lv_freertos\\.c")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/osal/lv_linux\\.c")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/osal/lv_mqx\\.c")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/osal/lv_pthread\\.c")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/osal/lv_rtthread\\.c")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/osal/lv_sdl2\\.c")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/osal/lv_windows\\.c")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/stdlib/clib/.*")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/stdlib/micropython/.*")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/stdlib/rtthread/.*")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/stdlib/uefi/.*")
list(FILTER LVGL_CORE_SOURCES EXCLUDE REGEX ".*/src/widgets/lottie/.*")

add_library(lvgl STATIC ${LVGL_CORE_SOURCES})

target_include_directories(lvgl PUBLIC
    ${LVGL_ROOT}
    ${LVGL_ROOT}/src
)

target_compile_definitions(lvgl PUBLIC
    LV_CONF_INCLUDE_SIMPLE
)
