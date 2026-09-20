cmake_minimum_required(VERSION 3.28)

# Synapse-X View — UDP 收帧 + D3D11 显示（无 CUDA）
#   cd view
#   cmake --preset windows-x64
#   cmake --build build_x64 --config RelWithDebInfo

# 单独 cd view 时才 project；根工程 add_subdirectory 时沿用顶层 SynapseX
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SynapseX_View
        VERSION 1.0.0
        DESCRIPTION "UDP frame viewer for latency checks"
        LANGUAGES C CXX
    )

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    if(WIN32)
        add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    endif()

    if(MSVC)
        add_compile_options(/utf-8 /W3 /permissive- /Zc:__cplusplus /Zc:preprocessor)
        add_compile_definitions(
            _CRT_SECURE_NO_WARNINGS
            $<$<CONFIG:Release>:NDEBUG>
        )
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Gy /Oi")
    endif()
endif()

if(NOT TARGET lz4_static)
    set(LZ4_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/lz4-1.10.0/lib")
    add_library(lz4_static STATIC
        "${LZ4_SRC_DIR}/lz4.c"
        "${LZ4_SRC_DIR}/lz4.h"
    )
    target_include_directories(lz4_static PUBLIC "${LZ4_SRC_DIR}")
    if(MSVC)
        target_compile_options(lz4_static PRIVATE /W0)
    endif()
endif()

if(NOT TARGET SynapseX_Shared)
    add_library(SynapseX_Shared INTERFACE)
    target_include_directories(SynapseX_Shared INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/spdlog-1.17.0/include
    )
    target_compile_features(SynapseX_Shared INTERFACE cxx_std_17)
    target_compile_definitions(SynapseX_Shared INTERFACE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
endif()

add_executable(SynapseX_View
    src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/src/UdpReceiver.cpp
)
target_include_directories(SynapseX_View PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
)
target_link_libraries(SynapseX_View PRIVATE
    SynapseX_Shared
    lz4_static
    d3d11
    dxgi
    d3dcompiler
    ws2_32
)
