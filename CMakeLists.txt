project(blospray)

# Might be able to lower this, but e.g.
# 3.9 does not have a cmake module to find protobuf
cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 11)

find_package(ospray 1.7 REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)
#find_package(glm REQUIRED)
find_library(OIIO OpenImageIO)

find_package(Protobuf REQUIRED)

if(PROTOBUF_FOUND)
    message ("protobuf found")
else()
    message (FATAL_ERROR "Cannot find Protobuf")
endif()

include_directories(${OSPRAY_INCLUDE_DIRS})
#include_directories(${Boost_INCLUDE_DIR})
#include_directories(${GLM_INCLUDE_DIRS})

# Server
add_subdirectory(core)

# Plugins
add_subdirectory(plugins)

