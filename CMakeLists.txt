cmake_minimum_required(VERSION 3.16)
project(c_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
add_compile_options(-Wall -Wextra)

# 可执行文件
add_executable(folder_similarity src/folder_similarity/main.cpp)
add_executable(oldsort src/oldsort/main.cpp)
add_executable(extract_name src/extract_name/main.cpp)

# 设置 include 目录
target_include_directories(folder_similarity PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(oldsort PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(extract_name PRIVATE ${CMAKE_SOURCE_DIR}/src)

# 安装配置
include(GNUInstallDirs)
install(TARGETS folder_similarity oldsort extract_name
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# 测试配置
enable_testing()

add_executable(test_runner tests/test_common.cpp external/catch_amalgamated.cpp)
target_include_directories(test_runner PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR})

add_test(NAME CommonTests COMMAND test_runner)

# ==================== 性能测试 (Celero) ====================

set(CELERO_ENABLE_EXPERIMENTS OFF CACHE BOOL "" FORCE)
set(CELERO_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/Celero-2.10.0)

add_executable(benchmark_runner benchmarks/benchmark_common.cpp)
target_include_directories(benchmark_runner PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(benchmark_runner PRIVATE celero)
