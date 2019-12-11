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

# https://stackoverflow.com/a/45843676/9296788 has a nice overview of 
# setting up a cmake project

# Might be able to lower this, but e.g.
# 3.9 does not have a cmake module to find protobuf
cmake_minimum_required(VERSION 3.12)

cmake_policy(SET CMP0048 NEW)   # Project version

set(BLOSPRAY_VERSION_MAJOR 0)
set(BLOSPRAY_VERSION_MINOR 1)

project(blospray
    VERSION "${BLOSPRAY_VERSION_MAJOR}.${BLOSPRAY_VERSION_MINOR}"
    DESCRIPTION "Blender external rendering engine using Intel OSPRay")


option(BUILD_FAKER "Build faker shared library" ON)
option(PLUGIN_COSMOGRID "Build cosmogrid scene plugin (needs uhdf5 and HDF5)" OFF)
option(PLUGIN_PLY "Build PLY geometry plugin (needs rply 1.1.4 sources)" OFF)
option(PLUGIN_ASSIMP "Build Assimp geometry plugin (needs Assimp)" OFF)
option(PLUGIN_VOLUME_HDF5 "Build PLY geometry plugin (needs uhdf5 and HDF5)" OFF)
option(PLUGIN_DISNEY_CLOUD "Build disney cloud volume plugin (needs OpenVDB)" OFF)
option(PLUGIN_VTK_STREAMLINES "Build VTK streamlines geometry plugin" OFF)
option(ADDRESS_SANITIZER "Compile with GCC's AddressSanitizer" OFF)
option(VTK_QC_BOUND "Add support to generate a simplified bound using VTK" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmakemodules")
list(APPEND CMAKE_MODULE_PATH "/usr/lib/cmake/OpenVDB")

include(FindOpenEXR)
include(FindOpenImageIO)
if(PLUGIN_DISNEY_CLOUD)
    find_package(OpenVDB REQUIRED)
endif(PLUGIN_DISNEY_CLOUD)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(PkgConfig)

find_package(ospray CONFIG 2.0.0 REQUIRED)
find_package(Boost COMPONENTS program_options regex REQUIRED)
find_package(glm REQUIRED)

if(NOT Boost_FOUND)
    message (FATAL_ERROR "Cannot find Boost library")
endif()

# XXX check for glm

# Screw it, cmake's FindProtobuf module can not find protobuf in 
# a non-standard place. Use pkg-config instead.
pkg_check_modules(PROTOBUF REQUIRED protobuf)

if(NOT PROTOBUF_FOUND)
    message (FATAL_ERROR "Cannot find Protobuf")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wunused")

# ASAN_OPTIONS=halt_on_error=0
if (ADDRESS_SANITIZER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize-recover=address -O0 -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize-recover=address -O0 -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize-recover=address -fno-omit-frame-pointer")
endif(ADDRESS_SANITIZER)

# Plugin dependencies

if(PLUGIN_COSMOGRID OR PLUGIN_VOLUME_HDF5)
    FIND_PACKAGE(HDF5)
    IF (HDF5_IS_PARALLEL)
        FIND_PACKAGE(MPI)
    ENDIF()

    INCLUDE_DIRECTORIES (${HDF5_INCLUDE_DIRS})
    IF (HDF5_IS_PARALLEL)
        INCLUDE_DIRECTORIES (${MPI_CXX_INCLUDE_PATH})
        SET(HDF5LIBS "${HDF5_LIBRARIES}" "${MPI_CXX_LIBRARIES}")
    ELSE()
        SET(HDF5LIBS "${HDF5_LIBRARIES}")
    ENDIF()
endif(PLUGIN_COSMOGRID OR PLUGIN_VOLUME_HDF5)

if(PLUGIN_ASSIMP)
    FIND_PACKAGE(assimp)
endif(PLUGIN_ASSIMP)

if(VTK_QC_BOUND OR PLUGIN_VTK_STREAMLINES)
    # https://lorensen.github.io/VTKExamples/site/Cxx/Meshes/QuadricClustering/
    FIND_PACKAGE(VTK
        COMPONENTS
        vtkCommonCore
        vtkCommonDataModel
        vtkFiltersCore
        vtkIOCore
        vtkIOLegacy             # vtkDataSetReader
        vtkRenderingCore        # vtkColorTransferFunction
        )

    if(NOT VTK_FOUND)
        message (FATAL_ERROR "Cannot find VTK: ${VTK_NOT_FOUND_MESSAGE}")
    endif()

    message (STATUS "VTK_VERSION: ${VTK_VERSION}")
endif(VTK_QC_BOUND OR PLUGIN_VTK_STREAMLINES)

include_directories("${CMAKE_SOURCE_DIR}/core")		# json.hpp in faker
include_directories(${OSPRAY_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIR})
include_directories(${GLM_INCLUDE_DIRS})

# Config file

configure_file (
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )

include_directories("${PROJECT_BINARY_DIR}")

# Library
add_subdirectory(core)

# Server
add_subdirectory(server)

# Plugins
add_subdirectory(plugins)

# Tests
add_subdirectory(tests)

# Faker
if(BUILD_FAKER)
    add_subdirectory(faker)
endif(BUILD_FAKER)
