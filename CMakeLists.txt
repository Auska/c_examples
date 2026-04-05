cmake_minimum_required(VERSION 3.16)
project(c_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
add_compile_options(-Wall -Wextra)

# 可执行文件
add_executable(folder_similarity folder_similarity.cpp)
add_executable(oldsort oldsort.cpp)
add_executable(extract_name extract_name.cpp)

# 安装配置
include(GNUInstallDirs)
install(TARGETS folder_similarity oldsort extract_name
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# 测试配置
enable_testing()

add_executable(test_runner tests/test_common.cpp external/catch_amalgamated.cpp)
target_include_directories(test_runner PRIVATE ${CMAKE_SOURCE_DIR})

add_test(NAME CommonTests COMMAND test_runner)
