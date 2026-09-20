cmake_minimum_required(VERSION 3.28)
project(SynapseX_Test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CUDA — CLion 经常看不到 CUDA_PATH，先按默认安装目录找
if(NOT CUDAToolkit_ROOT AND NOT DEFINED ENV{CUDAToolkit_ROOT} AND NOT DEFINED ENV{CUDA_PATH})
    foreach(_ver IN ITEMS 13.4 13.3 13.2 13.1 12.8 12.6)
        set(_cand "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v${_ver}")
        if(EXISTS "${_cand}/bin/nvcc.exe")
            set(CUDAToolkit_ROOT "${_cand}")
            break()
        endif()
    endforeach()
endif()

find_package(CUDAToolkit REQUIRED)
set(CUDA_ROOT "${CUDAToolkit_TARGET_DIR}")
set(CUDA_INC   "${CUDA_ROOT}/include")
set(CUDA_LIB   "${CUDA_ROOT}/lib/x64")

# TensorRT
set(TRT_ROOT "C:/Program Files/NVIDIA/TensorRT-10.16.1.11.Windows.amd64.cuda-13.2/TensorRT-10.16.1.11")
set(TRT_INC   "${TRT_ROOT}/include")
set(TRT_LIB   "${TRT_ROOT}/lib")

# Inference sources (from parent client project)
set(CLIENT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src")
set(CLIENT_INC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set(SHARED_INC "${CMAKE_CURRENT_SOURCE_DIR}/../../shared/include")

add_executable(test_infer
    src/main.cpp
    ${CLIENT_SRC}/TrtInference.cpp
    ${CLIENT_SRC}/CudaPreprocess.cpp
)

target_include_directories(test_infer PRIVATE
    include
    ${CLIENT_INC}
    ${SHARED_INC}
    ${CUDA_INC}
    ${TRT_INC}
)

target_compile_definitions(test_infer PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    UNICODE
    _UNICODE
    "SX_MODEL_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../model/engine\""
)

if(MSVC)
    target_compile_options(test_infer PRIVATE /utf-8 /W3)
endif()

target_link_libraries(test_infer
    ${TRT_LIB}/nvinfer_10.lib
    ${TRT_LIB}/nvinfer_plugin_10.lib
    ${CUDA_LIB}/cudart.lib
    ${CUDA_LIB}/cuda.lib
    ${CUDA_LIB}/nvrtc.lib
    windowscodecs    # WIC
    ole32            # COM
)

set(_sx_cuda_bin "${CUDAToolkit_TARGET_DIR}/bin")
if(EXISTS "${CUDAToolkit_TARGET_DIR}/bin/x64")
    set(_sx_cuda_bin "${CUDAToolkit_TARGET_DIR}/bin/x64")
endif()
file(GLOB SX_TRT_RUNTIME_DLLS "${TRT_ROOT}/bin/*.dll")
file(GLOB SX_CUDA_RUNTIME_DLLS
    "${_sx_cuda_bin}/nvrtc*.dll"
    "${_sx_cuda_bin}/cudart64*.dll"
    "${_sx_cuda_bin}/cublas64*.dll"
    "${_sx_cuda_bin}/cublasLt64*.dll"
    "${_sx_cuda_bin}/nvJitLink*.dll"
)
if(SX_TRT_RUNTIME_DLLS OR SX_CUDA_RUNTIME_DLLS)
    add_custom_command(TARGET test_infer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SX_TRT_RUNTIME_DLLS}
            ${SX_CUDA_RUNTIME_DLLS}
            "$<TARGET_FILE_DIR:test_infer>"
        COMMENT "Copying TensorRT/CUDA runtime DLLs"
    )
endif()
