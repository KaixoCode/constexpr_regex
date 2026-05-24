
# ==============================================

cmake_minimum_required(VERSION 3.15)

# ==============================================

set (CMAKE_CXX_STANDARD 23)

project(constexpr_regex)

# ==============================================

file(GLOB_RECURSE CONSTEXPR_REGEX_EXAMPLE_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/example/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/example/*.hpp"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CONSTEXPR_REGEX_EXAMPLE_SOURCE})

add_executable(constexpr_regex_example ${CONSTEXPR_REGEX_EXAMPLE_SOURCE})
target_include_directories(constexpr_regex_example
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ==============================================
