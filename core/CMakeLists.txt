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

# Generate the .h and .cxx files
#PROTOBUF_GENERATE_CPP(PROTO_CPP_SRCS PROTO_CPP_HDRS messages.proto)
# Python module as well
#PROTOBUF_GENERATE_PYTHON(PROTO_PYTHON_SRCS messages.proto)

set(PROTO_SOURCE "${CMAKE_SOURCE_DIR}/core/messages.proto")
set(PROTO_CPP_CPP "${CMAKE_BINARY_DIR}/messages.pb.cc")
set(PROTO_CPP_H "${CMAKE_BINARY_DIR}/messages.pb.h")
set(PROTO_PYTHON "${CMAKE_SOURCE_DIR}/render_ospray/messages_pb2.py")

add_custom_command(
    OUTPUT 
        "${PROTO_CPP_CPP}" "${PROTO_CPP_H}"
    COMMAND
        "${PROTOBUF_PREFIX}/bin/protoc" 
        "--cpp_out=${CMAKE_BINARY_DIR}" 
        "--python_out=${CMAKE_SOURCE_DIR}/render_ospray" 
        "--proto_path=${CMAKE_SOURCE_DIR}/core" 
        "messages.proto"
    DEPENDS 
        "${PROTO_SOURCE}"
    COMMENT 
        "Generating C++ and Python protobuf sources"
    VERBATIM)

add_library(libblospray
    SHARED
    bounding_mesh.cpp
    image.cpp
    ${PROTO_CPP_CPP})

set_target_properties(libblospray
    PROPERTIES
    OUTPUT_NAME blospray
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER plugin.h
    INSTALL_RPATH "\\\$ORIGIN"
    )
    
target_include_directories(libblospray
    PUBLIC
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(libblospray
    PUBLIC
    dl
    Threads::Threads
    ${OSPRAY_LIBRARIES} 
    ospray::ospray_testing
    ${OPENIMAGEIO_LIBRARIES} 
    ${Boost_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
)

# Installation (including setting rpath)

install(TARGETS 
    libblospray 
    DESTINATION bin)

#install(FILES
#    ${PROTO_PYTHON_SRCS}
#    DESTINATION
#    ${CMAKE_SOURCE_DIR})
