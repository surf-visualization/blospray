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

# geometry_unit_square

add_library(geometry_plane SHARED geometry_unit_plane.cpp)
set_target_properties(geometry_plane PROPERTIES PREFIX "")   
target_link_libraries(geometry_plane PUBLIC ${OSPRAY_LIBRARIES})

# geometry_rbc

add_library(geometry_rbc SHARED geometry_rbc.cpp)
set_target_properties(geometry_rbc PROPERTIES PREFIX "")   
target_link_libraries(geometry_rbc PUBLIC ${OSPRAY_LIBRARIES})

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
        ${PROTOBUF_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}    
    )

endif(PLUGIN_COSMOGRID)

install(TARGETS 
    volume_raw 
    geometry_plane
    geometry_rbc
    DESTINATION bin)
    

if(PLUGIN_COSMOGRID)

    install(TARGETS 
        geometry_cosmogrid 
        DESTINATION bin)

endif(PLUGIN_COSMOGRID)
