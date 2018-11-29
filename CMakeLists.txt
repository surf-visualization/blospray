project(blospray)

# Can probably lower this, but no idea to what
cmake_minimum_required(VERSION 3.12)

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
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS messages.proto)

# Print path to generated files
message ("PROTO_SRCS = ${PROTO_SRCS}")
message ("PROTO_HDRS = ${PROTO_HDRS}")

include_directories(${OSPRAY_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
include_directories(${GLM_INCLUDE_DIRS})

#add_executable(tutorial ospTutorial.cpp)
#link_libraries(tutorial ${OSPRAY_BUILD_DIR})
#target_link_libraries(tutorial ${OSPRAY_LIBRARIES})

add_executable(ospray_render_server 
    ospray_render_server.cpp 
    ${PROTO_SRCS}
    ${PROTO_HDRS})
    
target_include_directories(ospray_render_server
    PUBLIC
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(ospray_render_server
    PUBLIC
    ${OSPRAY_LIBRARIES} 
    ${OIIO} 
    ${Boost_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
)
