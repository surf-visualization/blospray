include_directories(${CMAKE_SOURCE_DIR}/core)

add_executable(t_json
    t_json.cpp)
    
    
install(TARGETS 
    t_json 
    DESTINATION bin)
