cmake_minimum_required(VERSION 3.5)
project(FFMPEG2)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )

add_definitions("-D_GLIBCXX_USE_CXX11_ABI=0")

set(SOURCE_FILES main.cpp)

include_directories("/home/prodvd/ClionProjects/FFMPEG2/")

add_executable(FFMPEG2 ${SOURCE_FILES})

target_link_libraries(FFMPEG2 "/usr/local/lib/libavformat.a" "/usr/local/lib/libavcodec.a"
        "/usr/local/lib/libavutil.a" "/usr/local/lib/libx264.a" "z" "pthread")
