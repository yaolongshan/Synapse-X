cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SynapseX_Sender VERSION 1.0.0 LANGUAGES C CXX)
endif()

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "SynapseX Sender 需要 Windows x64 MSVC 工具链")
endif()

# 专属 /MT LZ4，不改变 Host、Client、View 使用的库和运行库设置。
add_library(SynapseX_Sender_Lz4 STATIC ../thirdparty/lz4-1.10.0/lib/lz4.c)
target_include_directories(SynapseX_Sender_Lz4 PUBLIC ../thirdparty/lz4-1.10.0/lib)
set_property(TARGET SynapseX_Sender_Lz4 PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(SynapseX_Sender WIN32
    sender.manifest
    src/main.cpp
    src/SenderConfig.cpp
    src/SenderLog.cpp
    src/SenderSession.cpp
    ../host/src/DxgiCapturer.cpp
    ../host/src/Lz4Compressor.cpp
    ../host/src/UdpSender.cpp
)
set_target_properties(SynapseX_Sender PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
target_include_directories(SynapseX_Sender PRIVATE
    include ../host/include ../shared/include ../thirdparty/spdlog-1.17.0/include)
target_compile_definitions(SynapseX_Sender PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    _WIN32_WINNT=0x0A00 WINVER=0x0A00
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
target_compile_options(SynapseX_Sender PRIVATE /utf-8 /W3 /permissive- /Zc:__cplusplus /Zc:preprocessor)
target_link_libraries(SynapseX_Sender PRIVATE
    SynapseX_Sender_Lz4 d3d11 dxgi ws2_32 winmm user32 gdi32 shell32 ole32)
