project(wstream C CXX)
cmake_minimum_required(VERSION 3.3.2)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENABLE_ULTRALIGHT ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -Werror -I/usr/include/x86_64-linux-gnu")

set(SOURCES
    "streamer.cc"
    "wstream.cc")

if (${ENABLE_ULTRALIGHT})
    set(SOURCES ${SOURCES} "ultralight/ul.cc")
    include(ultralight/ultralight.cmake)
    ultralight_prepare(wstream)
endif ()

add_executable(wstream WIN32 MACOSX_BUNDLE ${SOURCES})
target_link_libraries(wstream PRIVATE Threads::Threads
    avcodec avutil avformat swscale)

if (${ENABLE_ULTRALIGHT})
    ultralight_after(wstream)
endif ()
