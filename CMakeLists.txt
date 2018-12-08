project(blospray)

# Can probably lower this, but no idea to what
cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 11)

find_package(ospray 1.6 REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)
find_package(glm REQUIRED)
find_library(OIIO OpenImageIO)

find_package(Protobuf REQUIRED)

if(PROTOBUF_FOUND)
    message ("protobuf found")
else()
    message (FATAL_ERROR "Cannot find Protobuf")
endif()

# Generate the .h and .cxx files
PROTOBUF_GENERATE_CPP(PROTO_CPP_SRCS PROTO_CPP_HDRS messages.proto)
# Python module as well
PROTOBUF_GENERATE_PYTHON(PROTO_PYTHON_SRCS messages.proto)

# Print path to generated files
message ("PROTO_CPP_SRCS = ${PROTO_CPP_SRCS}")
message ("PROTO_CPP_HDRS = ${PROTO_CPP_HDRS}")
# XXX for some strange reason no python file is generated
#message ("PROTO_PYTHON_SRCS = ${PROTO_PYTHON_SRCS}")

include_directories(${OSPRAY_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIR})
include_directories(${GLM_INCLUDE_DIRS})

# Server

add_executable(ospray_render_server 
    ospray_render_server.cpp 
    image.cpp
    ${PROTO_CPP_SRCS}
    ${PROTO_CPP_HDRS})
    
set_target_properties(ospray_render_server
    PROPERTIES
    INSTALL_RPATH "\\\$ORIGIN")
    
target_include_directories(ospray_render_server
    PUBLIC
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(ospray_render_server
    PUBLIC
    dl
    ${OSPRAY_LIBRARIES} 
    ${OIIO} 
    ${Boost_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
)

# Plugins

add_library(voltype_raw SHARED voltype_raw.cpp)
set_target_properties(voltype_raw PROPERTIES PREFIX "")   
target_link_libraries(voltype_raw PUBLIC ${OSPRAY_LIBRARIES})

target_include_directories(voltype_raw
    PUBLIC
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Installation (including setting rpath)

install(TARGETS 
    ospray_render_server voltype_raw
    DESTINATION bin)
    
#install(FILES
#    ${PROTO_PYTHON_SRCS}
#    DESTINATION
#    ${CMAKE_SOURCE_DIR})
