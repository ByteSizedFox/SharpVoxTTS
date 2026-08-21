cmake_minimum_required(VERSION 3.25)
project(sharpvox)

file(GLOB SOURCES src/*.cpp platform/lib/*.cpp)

add_library(sharpvox STATIC ${SOURCES})
target_include_directories(sharpvox PUBLIC include)
target_compile_definitions(sharpvox PUBLIC HAVE_MECAB)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(sharpvox PUBLIC m)
endif()

file(GLOB CLI_SOURCES platform/cli/*.cpp)

add_executable(sharpvox-cli ${CLI_SOURCES})
set_target_properties(sharpvox-cli PROPERTIES OUTPUT_NAME "sharpvox")
target_link_libraries(sharpvox-cli PRIVATE sharpvox)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	find_package(ALSA)

	if(${ALSA_FOUND})
		target_compile_definitions(sharpvox-cli PRIVATE HAVE_ALSA)
		target_link_libraries(sharpvox-cli PRIVATE ALSA::ALSA)
	endif()
endif()
