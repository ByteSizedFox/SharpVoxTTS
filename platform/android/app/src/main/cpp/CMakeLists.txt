cmake_minimum_required(VERSION 3.22.0)

project("sharpvox")

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../")
file(GLOB ENGINE_SOURCES "${REPO_ROOT}/src/*.cpp")
file(GLOB LIB_SOURCES "${REPO_ROOT}/platform/lib/*.cpp")

add_library(${CMAKE_PROJECT_NAME} SHARED
        ${ENGINE_SOURCES}
        ${LIB_SOURCES}
        native-lib.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
        "${REPO_ROOT}/include"
        "${REPO_ROOT}/platform/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        -std=c++17
        -DNO_FILESYSTEM
)

target_link_libraries(${CMAKE_PROJECT_NAME}
        android
        log)
