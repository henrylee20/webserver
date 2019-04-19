cmake_minimum_required(VERSION 3.11)
project(http_server)

set(CMAKE_CXX_STANDARD 11)

add_executable(http_server main.cpp http.cpp http.h http_listener.cpp http_listener.h queue.h thread_pool.h ref_string.h ref_string.cpp cache.h http_service.cpp http_service.h utils.h utils.cpp)

target_link_libraries(http_server pthread)