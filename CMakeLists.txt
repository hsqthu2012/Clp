cmake_minimum_required(VERSION 3.10)
project(clp LANGUAGES CXX C)

# use C++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


file(GLOB SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(clp ${SOURCE_FILES})
target_include_directories(clp PUBLIC src)