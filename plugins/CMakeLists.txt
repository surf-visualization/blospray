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

include_directories(${CMAKE_SOURCE_DIR}/core)

# For protobuf header
include_directories(${CMAKE_BINARY_DIR})

# volume_raw

add_library(volume_raw SHARED volume_raw.cpp)
set_target_properties(volume_raw PROPERTIES PREFIX "")   
target_link_libraries(volume_raw PUBLIC ${OSPRAY_LIBRARIES})
target_include_directories(volume_raw
    PUBLIC
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# volume_testing

add_library(volume_testing SHARED volume_testing.cpp)
set_target_properties(volume_testing PROPERTIES PREFIX "")   
target_link_libraries(volume_testing PUBLIC ${OSPRAY_LIBRARIES} ospray::ospray_testing)
target_include_directories(volume_testing
    PUBLIC
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# geometry_plane

add_library(geometry_plane SHARED geometry_plane.cpp)
set_target_properties(geometry_plane PROPERTIES PREFIX "")   
target_link_libraries(geometry_plane PUBLIC ${OSPRAY_LIBRARIES})

# scene_rbc

add_library(scene_rbc SHARED scene_rbc.cpp)
set_target_properties(scene_rbc PROPERTIES PREFIX "")   
target_link_libraries(scene_rbc PUBLIC ${OSPRAY_LIBRARIES})

if(PLUGIN_COSMOGRID)

    # geometry_cosmogrid

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

    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11")

    INCLUDE_DIRECTORIES (${HDF5_INCLUDE_DIRS})
    INCLUDE_DIRECTORIES (${MPI_CXX_INCLUDE_PATH})

    add_library(geometry_cosmogrid SHARED geometry_cosmogrid.cpp)
    set_target_properties(geometry_cosmogrid PROPERTIES PREFIX "")   
    target_link_libraries(geometry_cosmogrid PUBLIC ${OSPRAY_LIBRARIES} ${HDF5LIBS})
    target_include_directories(geometry_cosmogrid
        PUBLIC
        /home/paulm/projects/uhdf5-git
        /home/melis/projects/uhdf5
        ${PROTOBUF_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}    
    )

endif(PLUGIN_COSMOGRID)

install(TARGETS 
    volume_raw 
    volume_testing
    geometry_plane
    scene_rbc
    DESTINATION bin)
    

if(PLUGIN_COSMOGRID)

    install(TARGETS 
        geometry_cosmogrid 
        DESTINATION bin)

endif(PLUGIN_COSMOGRID)
