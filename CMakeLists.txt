cmake_minimum_required(VERSION 3.25)
project(sharpvox)

file(GLOB SOURCES src/*.cpp)

add_library(sharpvox STATIC ${SOURCES})
target_include_directories(sharpvox PUBLIC include)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(sharpvox PUBLIC m)
endif()
