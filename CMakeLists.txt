project(blospray)

# Can probably lower this, but no idea to what
cmake_minimum_required(VERSION 3.12)

find_package(ospray 1.6 REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)
find_package(glm REQUIRED)

include_directories(${OSPRAY_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
include_directories(${GLM_INCLUDE_DIRS})

add_executable(tutorial ospTutorial.cpp)

link_libraries(tutorial ${OSPRAY_BUILD_DIR})
target_link_libraries(tutorial ${OSPRAY_LIBRARIES})


add_executable(ospray_render_server ospray_render_server.cpp)