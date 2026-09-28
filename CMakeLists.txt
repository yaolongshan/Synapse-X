cmake_minimum_required(VERSION 3.28)
project(SynapseX
    VERSION 1.0.0
    DESCRIPTION "Host + Client + View + OxStream — CLion 打开仓库根即可选择各模块"
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

add_subdirectory(shared)
add_subdirectory(host)
add_subdirectory(OxStream)
add_subdirectory(view)
add_subdirectory(client)
