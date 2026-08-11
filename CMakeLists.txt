cmake_minimum_required(VERSION 3.21)
project(H264Tool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 仅面向 Windows x64
if(NOT WIN32)
    message(FATAL_ERROR "H264Tool 仅支持 Windows 平台")
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "H264Tool 仅支持 x64 构建")
endif()

# Qt6（Widgets + Gui）
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

# 可选 FFmpeg：用于容器解复用与软解码预览
# 默认关闭：无 FFmpeg 亦可完成解析与条带图；需要预览时再 -DH264TOOL_USE_FFMPEG=ON
option(H264TOOL_USE_FFMPEG "启用 FFmpeg 解码与容器读取" OFF)
set(H264TOOL_FFMPEG_ROOT "" CACHE PATH "FFmpeg 安装根目录（含 include/lib）")

set(CORE_SOURCES
    src/core/BitReader.cpp
    src/core/NalUnit.cpp
    src/core/NalParser.cpp
    src/core/SpsPpsParser.cpp
    src/core/SliceHeaderParser.cpp
    src/core/FrameInfo.cpp
    src/core/StreamAnalyzer.cpp
    src/core/RawH264Reader.cpp
    src/core/Mp4Demuxer.cpp
    src/core/AviDemuxer.cpp
    src/core/FileLoader.cpp
)

set(DECODE_SOURCES
    src/decode/DecoderFactory.cpp
    src/decode/NullDecoder.cpp
)

set(UI_SOURCES
    src/ui/MainControlWindow.cpp
    src/ui/PreviewWindow.cpp
    src/ui/FrameBarChart.cpp
    src/ui/FrameInfoPanel.cpp
    src/ui/StatsPanel.cpp
    src/ui/NavigationBar.cpp
    src/ui/SyncController.cpp
)

set(APP_SOURCES
    src/main.cpp
    resources/app.qrc
)

if(WIN32)
    # windres 工作目录常在 build/，用绝对路径写入生成的 .rc
    set(H264TOOL_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/h264tool.ico")
    # 反斜杠转义供 RC 使用（正斜杠在 windres 中也可用）
    file(TO_CMAKE_PATH "${H264TOOL_ICON_PATH}" H264TOOL_ICON_PATH)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/app.rc
        @ONLY
    )
    list(APPEND APP_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
endif()

if(H264TOOL_USE_FFMPEG)
    list(APPEND DECODE_SOURCES src/decode/FFmpegDecoder.cpp)
endif()

add_executable(H264Tool WIN32
    ${APP_SOURCES}
    ${CORE_SOURCES}
    ${DECODE_SOURCES}
    ${UI_SOURCES}
)

target_include_directories(H264Tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(H264Tool PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
)

if(H264TOOL_USE_FFMPEG)
    # 约定：用户通过 H264TOOL_FFMPEG_ROOT 或系统 PATH 提供 FFmpeg
    if(H264TOOL_FFMPEG_ROOT)
        target_include_directories(H264Tool PRIVATE
            ${H264TOOL_FFMPEG_ROOT}/include
        )
        target_link_directories(H264Tool PRIVATE
            ${H264TOOL_FFMPEG_ROOT}/lib
        )
    endif()
    target_compile_definitions(H264Tool PRIVATE H264TOOL_HAS_FFMPEG=1)
    target_link_libraries(H264Tool PRIVATE
        avformat avcodec avutil swscale
    )
else()
    target_compile_definitions(H264Tool PRIVATE H264TOOL_HAS_FFMPEG=0)
endif()

# 生成 VS 工程时按模块分组
source_group("core" FILES ${CORE_SOURCES})
source_group("decode" FILES ${DECODE_SOURCES})
source_group("ui" FILES ${UI_SOURCES})
source_group("app" FILES ${APP_SOURCES})

install(TARGETS H264Tool RUNTIME DESTINATION bin)
