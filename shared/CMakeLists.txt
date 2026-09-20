# Shared — header-only（协议 + spdlog）
# 根工程 add_subdirectory(shared) 走这里；单独 cd host/client/view 时各目录自己建同名 INTERFACE。
if(NOT TARGET SynapseX_Shared)
    add_library(SynapseX_Shared INTERFACE)
    target_include_directories(SynapseX_Shared INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/spdlog-1.17.0/include
    )
    target_compile_features(SynapseX_Shared INTERFACE cxx_std_17)
    target_compile_definitions(SynapseX_Shared INTERFACE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
endif()
