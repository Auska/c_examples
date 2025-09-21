cmake_minimum_required(VERSION 3.0)
project(c_examples)

set(CMAKE_CXX_STANDARD 23)

include_directories(lib)

add_executable(c_examples
        folder_similarity.cpp)
