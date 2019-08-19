# ======================================================================== #
# BLOSPRAY - OSPRay as a Blender render engine                             #
# Paul Melis, SURFsara <paul.melis@surfsara.nl>                            #
# ======================================================================== #
# Copyright 2018-2019 SURFsara                                             #
#                                                                          #
# Licensed under the Apache License, Version 2.0 (the "License");          #
# you may not use this file except in compliance with the License.         #
# You may obtain a copy of the License at                                  #
#                                                                          #
#     http://www.apache.org/licenses/LICENSE-2.0                           #
#                                                                          #
# Unless required by applicable law or agreed to in writing, software      #
# distributed under the License is distributed on an "AS IS" BASIS,        #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. #
# See the License for the specific language governing permissions and      #
# limitations under the License.                                           #
# ======================================================================== #

project(blospray)

# Might be able to lower this, but e.g.
# 3.9 does not have a cmake module to find protobuf
cmake_minimum_required(VERSION 3.12)

option(PLUGIN_COSMOGRID "Build cosmogrid geometry plugin (needs uhdf5)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmakemodules")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(PkgConfig)

find_package(ospray CONFIG 2.0.0 REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)
#find_package(glm REQUIRED)

include(FindOpenImageIO)

if(NOT Boost_FOUND)
    message (FATAL_ERROR "Cannot find Boost library")
endif()

# Screw it, cmake's FindProtobuf module can not find protobuf in 
# a non-standard place. Use pkg-config instead.
pkg_check_modules(PROTOBUF REQUIRED protobuf)

if(PROTOBUF_FOUND)
    message ("protobuf found")
else()
    message (FATAL_ERROR "Cannot find Protobuf")
endif()

set(CMAKE_CXX_STANDARD 11)

include_directories(${OSPRAY_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIR})
#include_directories(${GLM_INCLUDE_DIRS})

# Server
add_subdirectory(core)

# Plugins
add_subdirectory(plugins)

# Tests
add_subdirectory(tests)
