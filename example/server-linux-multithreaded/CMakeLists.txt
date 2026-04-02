cmake_minimum_required(VERSION 3.16)
project(server)
set(CMAKE_CXX_STANDARD 14)

#TODO- Try and re-enable optimistaion
# Add -O0 to remove optimizations when using gcc
#IF(CMAKE_COMPILER_IS_GNUCC)
#	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
#	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
#ENDIF(CMAKE_COMPILER_IS_GNUCC)

add_subdirectory(cocoip)

add_executable(server main.cpp)

target_link_libraries(server PRIVATE cocoip_srv)
target_link_libraries(server PRIVATE pthread)