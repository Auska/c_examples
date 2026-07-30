cmake_minimum_required(VERSION 3.16)
project(c_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 构建选项
option(ENABLE_TESTS "Build unit tests" ON)
option(ENABLE_BENCHMARKS "Build benchmarks" ON)

# 编译选项
add_compile_options(-Wall -Wextra -Werror)

# ==================== 公共库 ====================

add_library(common_utils STATIC
    src/common/cli_args.cpp
    src/common/output_utils.cpp
    src/common/size_utils.cpp
    src/common/string_utils.cpp
    src/common/time_utils.cpp
    src/common/fs_utils.cpp
)
target_include_directories(common_utils PUBLIC ${CMAKE_SOURCE_DIR}/src)

# ==================== 可执行文件 ====================

add_executable(folder_similarity src/folder_similarity/main.cpp)
add_executable(oldsort src/oldsort/main.cpp)
add_executable(extract_name src/extract_name/main.cpp)

target_link_libraries(folder_similarity PRIVATE common_utils)
target_link_libraries(oldsort PRIVATE common_utils)
target_link_libraries(extract_name PRIVATE common_utils)

# RapidFuzz include — 仅 header-only，无需链接
target_include_directories(folder_similarity PRIVATE
    ${CMAKE_SOURCE_DIR}/external/rapidfuzz-cpp-3.3.3)
target_include_directories(oldsort PRIVATE
    ${CMAKE_SOURCE_DIR}/external/rapidfuzz-cpp-3.3.3)
target_include_directories(extract_name PRIVATE
    ${CMAKE_SOURCE_DIR}/external/rapidfuzz-cpp-3.3.3)

# 安装配置
include(GNUInstallDirs)
install(TARGETS folder_similarity oldsort extract_name
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ==================== 测试配置 ====================

if(ENABLE_TESTS)
  enable_testing()

  add_executable(test_runner
      tests/test_common.cpp
      tests/test_tools.cpp
      external/Catch2-3.15.3/extras/catch_amalgamated.cpp)
  target_include_directories(test_runner PRIVATE
      ${CMAKE_SOURCE_DIR}/external/Catch2-3.15.3/extras
      ${CMAKE_SOURCE_DIR}/external/rapidfuzz-cpp-3.3.3)
  target_link_libraries(test_runner PRIVATE common_utils)

  add_test(NAME CommonTests COMMAND test_runner)
endif()

# ==================== 性能测试 (Celero) ====================

if(ENABLE_BENCHMARKS)
  set(CELERO_ENABLE_EXPERIMENTS OFF CACHE BOOL "" FORCE)
  set(CELERO_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  add_subdirectory(external/Celero-2.10.0 EXCLUDE_FROM_ALL)

  add_executable(benchmark_runner benchmarks/benchmark_common.cpp)
  target_include_directories(benchmark_runner PRIVATE
      ${CMAKE_SOURCE_DIR}/external/rapidfuzz-cpp-3.3.3)
  target_link_libraries(benchmark_runner PRIVATE common_utils celero)
endif()
