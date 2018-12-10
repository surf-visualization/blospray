include_directories(${CMAKE_SOURCE_DIR}/core)

# For protobuf header
include_directories(${CMAKE_BINARY_DIR})

add_library(volume_raw SHARED volume_raw.cpp)
set_target_properties(volume_raw PROPERTIES PREFIX "")   
target_link_libraries(volume_raw PUBLIC ${OSPRAY_LIBRARIES})

target_include_directories(volume_raw
    PUBLIC
    ${PROTOBUF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

install(TARGETS 
    volume_raw
    DESTINATION bin)
