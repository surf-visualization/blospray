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

#add_library(volume_testing SHARED volume_testing.cpp)
#set_target_properties(volume_testing PROPERTIES PREFIX "")   
#target_link_libraries(volume_testing PUBLIC ${OSPRAY_LIBRARIES} ospray::ospray_testing)
#target_include_directories(volume_testing
#    PUBLIC
#    ${PROTOBUF_INCLUDE_DIRS}
#    ${CMAKE_CURRENT_BINARY_DIR}
#)

# geometry_plane (for testing)

add_library(geometry_plane SHARED geometry_plane.cpp)
set_target_properties(geometry_plane PROPERTIES PREFIX "")   
target_link_libraries(geometry_plane PUBLIC ${OSPRAY_LIBRARIES})
target_include_directories(geometry_plane
    PUBLIC
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# geometry_ply

if(PLUGIN_PLY)

    # XXX Rply path

    add_library(geometry_ply SHARED geometry_ply.cpp ${CMAKE_SOURCE_DIR}/rply-1.1.4/rply.c)
    set_target_properties(geometry_ply PROPERTIES PREFIX "")   
    target_link_libraries(geometry_ply PUBLIC ${OSPRAY_LIBRARIES})
    target_include_directories(geometry_ply
        PUBLIC
        ${CMAKE_SOURCE_DIR}/rply-1.1.4
        ${PROTOBUF_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
    )

endif(PLUGIN_PLY)

# geometry_assimp

if(PLUGIN_ASSIMP)

    add_library(geometry_assimp SHARED geometry_assimp.cpp)
    set_target_properties(geometry_assimp PROPERTIES PREFIX "")   
    target_link_libraries(geometry_assimp PUBLIC 
        ${OSPRAY_LIBRARIES}
        ${ASSIMP_LIBRARIES})
    target_include_directories(geometry_assimp
        PUBLIC
        ${ASSIMP_INCLUDE_DIRS}
        ${PROTOBUF_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
    )

endif(PLUGIN_ASSIMP)

# scene_rbc

add_library(scene_rbc SHARED scene_rbc.cpp)
set_target_properties(scene_rbc PROPERTIES PREFIX "")   
target_link_libraries(scene_rbc PUBLIC ${OSPRAY_LIBRARIES})
target_include_directories(scene_rbc
    PUBLIC
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)


# scene_cornellbox

#add_library(scene_cornellbox SHARED scene_cornellbox.cpp)
#set_target_properties(scene_cornellbox PROPERTIES PREFIX "")   
#target_link_libraries(scene_cornellbox PUBLIC ${OSPRAY_LIBRARIES} ospray::ospray_testing)
#target_include_directories(scene_cornellbox
#    PUBLIC
#    ${PROTOBUF_INCLUDE_DIRS}
#    ${CMAKE_CURRENT_BINARY_DIR}
#)

# scene_boxes

#add_library(scene_boxes SHARED scene_boxes.cpp)
#set_target_properties(scene_boxes PROPERTIES PREFIX "")   
#target_link_libraries(scene_boxes PUBLIC ${OSPRAY_LIBRARIES} ospray::ospray_testing)
#target_include_directories(scene_boxes
#    PUBLIC
#    ${PROTOBUF_INCLUDE_DIRS}
#    ${CMAKE_CURRENT_BINARY_DIR}
#)


if(PLUGIN_COSMOGRID)

    # scene_cosmogrid

    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11")

    add_library(scene_cosmogrid SHARED scene_cosmogrid.cpp)
    set_target_properties(scene_cosmogrid PROPERTIES PREFIX "")   
    target_link_libraries(scene_cosmogrid PUBLIC ${OSPRAY_LIBRARIES} ${HDF5LIBS})
    target_include_directories(scene_cosmogrid
        PUBLIC
        /home/paulm/projects/uhdf5-git
        /home/melis/projects/uhdf5
        ${PROTOBUF_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}    
    )

endif(PLUGIN_COSMOGRID)

if(PLUGIN_VOLUME_HDF5)
    add_library(volume_hdf5 SHARED volume_hdf5.cpp)
    set_target_properties(volume_hdf5 PROPERTIES PREFIX "")   
    target_link_libraries(volume_hdf5 PUBLIC ${OSPRAY_LIBRARIES} ${HDF5LIBS})
    target_include_directories(volume_hdf5
        PUBLIC
        /home/paulm/projects/uhdf5-git
        /home/melis/projects/uhdf5        
        ${PROTOBUF_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
    )
endif(PLUGIN_VOLUME_HDF5)

install(TARGETS 
    volume_raw 
    #volume_testing
    geometry_plane
    scene_rbc
    #scene_boxes
    #scene_cornellbox
    DESTINATION bin)
    

if(PLUGIN_COSMOGRID)

    install(TARGETS 
        scene_cosmogrid 
        DESTINATION bin)

endif(PLUGIN_COSMOGRID)

if(PLUGIN_PLY)

    install(TARGETS 
        geometry_ply 
        DESTINATION bin)

endif(PLUGIN_PLY)

if(PLUGIN_ASSIMP)

    install(TARGETS 
        geometry_assimp 
        DESTINATION bin)

endif(PLUGIN_ASSIMP)

if(PLUGIN_VOLUME_HDF5)

    install(TARGETS 
        volume_hdf5 
        DESTINATION bin)

endif(PLUGIN_VOLUME_HDF5)
