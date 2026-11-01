cmake_minimum_required(VERSION 3.20)
project(hourglass_browser LANGUAGES C CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Configure with emcmake; this target requires Emscripten.")
endif()

include(FetchContent)
FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v8.3.11
    GIT_SHALLOW TRUE
    # Populate sources without enabling LVGL's separate library/demo targets.
    SOURCE_SUBDIR browser-source-only
)
FetchContent_MakeAvailable(lvgl)

get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS "${lvgl_SOURCE_DIR}/src/*.c")
file(GLOB FONT_SOURCES CONFIGURE_DEPENDS "${ROOT}/src/ui/assets/fonts/*.c")
file(GLOB ICON_SOURCES CONFIGURE_DEPENDS "${ROOT}/src/ui/assets/icons/*.c")
file(GLOB UI_ICON_SOURCES CONFIGURE_DEPENDS "${ROOT}/src/ui/assets/ui_icons/*.c")

add_executable(hourglass
    ${LVGL_SOURCES} ${FONT_SOURCES} ${ICON_SOURCES} ${UI_ICON_SOURCES}
    "${ROOT}/src/diagnostics.cpp"
    "${ROOT}/src/app/app.cpp"
    "${ROOT}/src/app/input_router.cpp"
    "${ROOT}/src/services/air_rating.cpp"
    "${ROOT}/src/services/alarm.cpp"
    "${ROOT}/src/services/brightness.cpp"
    "${ROOT}/src/services/display_mode.cpp"
    "${ROOT}/src/services/effects.cpp"
    "${ROOT}/src/services/glance.cpp"
    "${ROOT}/src/services/sound.cpp"
    "${ROOT}/src/services/wifi_signal.cpp"
    "${ROOT}/src/services/sound_alarm_clip.cpp"
    "${ROOT}/src/ui/theme.cpp"
    "${ROOT}/src/ui/ui_icon.cpp"
    "${ROOT}/src/ui/navigator.cpp"
    "${ROOT}/src/ui/boot_overlay.cpp"
    "${ROOT}/src/ui/overlays.cpp"
    "${ROOT}/src/ui/weather_format.cpp"
    "${ROOT}/src/ui/weather_icons.cpp"
    "${ROOT}/src/ui/screens/clock_screen.cpp"
    "${ROOT}/src/ui/screens/weather_screen.cpp"
    main.cpp display.cpp runtime.cpp environment.cpp
)
target_include_directories(hourglass PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/compat" "${ROOT}/include" "${lvgl_SOURCE_DIR}")
target_compile_definitions(hourglass PRIVATE LV_CONF_INCLUDE_SIMPLE HG_SIMULATOR=1)
target_compile_features(hourglass PRIVATE cxx_std_17)
target_compile_options(hourglass PRIVATE -include "${ROOT}/include/lv_conf.h")
target_link_options(hourglass PRIVATE
    "-sMODULARIZE=1" "-sEXPORT_NAME=createHourglass" "-sENVIRONMENT=web"
    "-sALLOW_MEMORY_GROWTH=1" "-sASSERTIONS=1"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','UTF8ToString']"
)
set_target_properties(hourglass PROPERTIES SUFFIX ".js")
configure_file(web/index.html index.html COPYONLY)
configure_file(web/preview.js preview.js COPYONLY)
configure_file(web/test.js test.js COPYONLY)
