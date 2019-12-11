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

add_executable(blserver
    main.cpp)
    
set_target_properties(blserver
    PROPERTIES
    INSTALL_RPATH "\\\$ORIGIN")
    
target_include_directories(blserver
    PUBLIC
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(blserver
    PUBLIC
    libblospray
    dl
    Threads::Threads
    ospray::ospray
    ospray::ospray_testing
    ${OPENIMAGEIO_LIBRARIES}
    ${OPENEXR_LIBRARIES}
    ${Boost_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
)

# Installation (including setting rpath)

install(TARGETS 
    blserver 
    DESTINATION bin)

#install(FILES
#    ${PROTO_PYTHON_SRCS}
#    DESTINATION
#    ${CMAKE_SOURCE_DIR})
