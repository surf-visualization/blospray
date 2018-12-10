# Generate the .h and .cxx files
PROTOBUF_GENERATE_CPP(PROTO_CPP_SRCS PROTO_CPP_HDRS messages.proto)
# Python module as well
PROTOBUF_GENERATE_PYTHON(PROTO_PYTHON_SRCS messages.proto)

# Print path to generated files
message ("PROTO_CPP_SRCS = ${PROTO_CPP_SRCS}")
message ("PROTO_CPP_HDRS = ${PROTO_CPP_HDRS}")
# XXX for some strange reason no python file is generated
#message ("PROTO_PYTHON_SRCS = ${PROTO_PYTHON_SRCS}")

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

# Installation (including setting rpath)

install(TARGETS 
    ospray_render_server 
    DESTINATION bin)

#install(FILES
#    ${PROTO_PYTHON_SRCS}
#    DESTINATION
#    ${CMAKE_SOURCE_DIR})
