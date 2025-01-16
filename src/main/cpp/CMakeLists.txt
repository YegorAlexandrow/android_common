cmake_minimum_required(VERSION 3.18.1)

project("native_audio")

set(TIME_CHEAT 1)
set(FAKE_RESTART 0)

find_package(oboe REQUIRED CONFIG)

include(FetchContent)

FetchContent_Declare(fftw URL https://www.fftw.org/fftw-3.3.10.tar.gz)

FetchContent_MakeAvailable(fftw)

if (NOT TARGET fftw3)
        add_subdirectory(${fftw_SOURCE_DIR} ${CMAKE_BINARY_DIR}/fftw_build)
endif()

include_directories(${fftw_SOURCE_DIR}/api gcc_phat)

add_library(${PROJECT_NAME}
        SHARED
        ${CMAKE_SOURCE_DIR}/audio_jni.cpp
        ${CMAKE_SOURCE_DIR}/gcc_phat/GccPhat.cpp
        ${CMAKE_SOURCE_DIR}/gcc_phat/Fft.cpp
        )

target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        _LINUX
        _FAKE_RESTART=${FAKE_RESTART}
        )

target_link_libraries(${PROJECT_NAME}
        fftw3
        oboe::oboe
        ${CMAKE_SOURCE_DIR}/solical/libSoliCallSDK-debug.a
        $<IF:${TIME_CHEAT},${CMAKE_SOURCE_DIR}/time_wrapper.cpp,>
        log
        android
        )

target_link_options(${PROJECT_NAME}
        PRIVATE
        $<IF:${TIME_CHEAT},LINKER:-wrap=time,>
        )
