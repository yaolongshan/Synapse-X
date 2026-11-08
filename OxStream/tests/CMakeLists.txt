cmake_minimum_required(VERSION 3.16)
project(OxStreamPixelTests LANGUAGES C CXX)
enable_testing()
add_executable(oxstream_pixel_packing_tests pixel_packing_tests.cpp)
target_compile_features(oxstream_pixel_packing_tests PRIVATE cxx_std_17)
target_include_directories(oxstream_pixel_packing_tests PRIVATE ../../shared/include)
add_test(NAME oxstream_pixel_packing_tests COMMAND oxstream_pixel_packing_tests)

add_executable(oxstream_compression_buffer_tests
    compression_buffer_tests.cpp
    ../../host/src/Lz4Compressor.cpp
    ../../thirdparty/lz4-1.10.0/lib/lz4.c)
target_compile_features(oxstream_compression_buffer_tests PRIVATE cxx_std_17)
target_include_directories(oxstream_compression_buffer_tests PRIVATE
    ../../host/include ../../thirdparty/lz4-1.10.0/lib)
add_test(NAME oxstream_compression_buffer_tests COMMAND oxstream_compression_buffer_tests)

add_executable(oxstream_stage_timing_tests stage_timing_tests.cpp)
find_package(Threads REQUIRED)
target_link_libraries(oxstream_stage_timing_tests PRIVATE Threads::Threads)
target_compile_features(oxstream_stage_timing_tests PRIVATE cxx_std_17)
target_include_directories(oxstream_stage_timing_tests PRIVATE ../include)
add_test(NAME oxstream_stage_timing_tests COMMAND oxstream_stage_timing_tests)

add_executable(oxstream_dxgi_readback_scope_tests dxgi_readback_scope_tests.cpp)
target_compile_features(oxstream_dxgi_readback_scope_tests PRIVATE cxx_std_17)
target_include_directories(oxstream_dxgi_readback_scope_tests PRIVATE ../../host/include)
add_test(NAME oxstream_dxgi_readback_scope_tests COMMAND oxstream_dxgi_readback_scope_tests)
