# Android NDK build:  add to app/build.gradle  externalNativeBuild { cmake { path "CMakeLists.txt" } }
cmake_minimum_required(VERSION 3.18)
project(traffic LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
add_library(traffic SHARED
    jni_bridge.cpp
    ../traffic_c_api.cpp
    ${ROOT}/core/traffic_engine.cpp
    ${ROOT}/core/traffic_config.cpp)
