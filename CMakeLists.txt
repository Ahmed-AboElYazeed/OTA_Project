cmake_minimum_required(VERSION 3.16)
project(OtaUpdate VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED BUILD_TARGET)
    set(BUILD_TARGET "laptop")
endif()

message(STATUS "BUILD_TARGET = ${BUILD_TARGET}")

# OpenSSL is found the same way on both targets
find_package(OpenSSL REQUIRED)

if(BUILD_TARGET STREQUAL "laptop")
    # PkgConfig is available on the native laptop build
    find_package(PkgConfig REQUIRED)
    add_subdirectory(laptop)
elseif(BUILD_TARGET STREQUAL "rpi")
    # Do NOT call find_package(PkgConfig) here — not available in Yocto cross env
    add_subdirectory(rpi)
else()
    message(FATAL_ERROR "Unknown BUILD_TARGET '${BUILD_TARGET}'. Use 'laptop' or 'rpi'.")
endif()
