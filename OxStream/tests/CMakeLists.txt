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
