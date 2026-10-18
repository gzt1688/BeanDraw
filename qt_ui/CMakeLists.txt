cmake_minimum_required(VERSION 3.24)

project(BeanDraw VERSION 0.1.0 LANGUAGES CXX)

# Qt 6.12 头文件要求 C++20（6.7+ 亦可）；MSVC 2022+ / 较新 GCC / Clang 均支持
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 平台差异化编译宏（Windows / OpenHarmony）
include(cmake/PlatformConfig.cmake)

# Qt 版本要求：6.7+（Widgets API 稳定，兼容最新 6.12；无需改代码）
find_package(Qt6 6.7 COMPONENTS Widgets LinguistTools Concurrent PrintSupport REQUIRED)
qt_standard_project_setup()

# Rust 内核静态库（自动 cargo build 并链接）
include(cmake/FindRustCore.cmake)
craftmaster_add_rust_core()

# Qt Advanced Docking System（停靠面板框架，锁定 v5.1.1）
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)  # 不构建 ADS 示例 / demo
set(BUILD_STATIC ON CACHE BOOL "" FORCE)     # 静态库，免去额外 DLL 依赖
FetchContent_Declare(
  qtadvanceddocking
  # 直接使用本地源码（deps 下 v5.1.1 解压包）：不配置 GIT_REPOSITORY，
  # 避免 FetchContent 在 GitHub 网络不可用时反复 git clone/update 失败。
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../deps/Qt-Advanced-Docking-System-5.1.1
)
FetchContent_MakeAvailable(qtadvanceddocking)

file(GLOB_RECURSE CRAFTMASTER_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)
qt_add_executable(BeanDraw ${CRAFTMASTER_SOURCES})

target_include_directories(BeanDraw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(BeanDraw PRIVATE
  Qt6::Widgets
  Qt6::Concurrent
  Qt6::PrintSupport
  ${CRAFTMASTER_RUST_LIB}
  ${CRAFTMASTER_RUST_SYSTEM_LIBS}
  ads::qtadvanceddocking
)

# 关键：保证 Rust 静态库先于 Qt 链接完成（首次构建必须）
add_dependencies(BeanDraw craftmaster_rust_core)

# ADS 头文件兜底：其公共头位于源码 src/ 目录
if(TARGET qtadvanceddocking)
  target_include_directories(BeanDraw PRIVATE ${qtadvanceddocking_SOURCE_DIR}/src)
endif()

# 多语言（lrelease 生成 qm，输出到 <build>/translations）
qt_add_translations(BeanDraw
  TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/i18n/translations/beandraw_zh_CN.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/i18n/translations/beandraw_en_US.ts
)

# 样式表资源
qt_add_resources(BeanDraw "beandraw_assets"
  PREFIX "/"
  BASE ${CMAKE_CURRENT_SOURCE_DIR}/assets
  FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/qss/beandraw.qss
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/qss/beandraw_dark.qss
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/arrow_down_dark.png
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/arrow_down_light.png
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/arrow_up_dark.png
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/arrow_up_light.png
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/radio_dot_dark.png
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/radio_dot_light.png
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/indicator_empty.png
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/favicon.ico
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/donate.png
)

target_compile_definitions(BeanDraw PRIVATE ${CRAFTMASTER_PLATFORM_DEFINITIONS})

# Windows：将图标嵌入可执行文件（资源管理器 / 任务栏图标）
if(WIN32)
  target_sources(BeanDraw PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/appicon.rc")
endif()

install(TARGETS BeanDraw RUNTIME DESTINATION .)
