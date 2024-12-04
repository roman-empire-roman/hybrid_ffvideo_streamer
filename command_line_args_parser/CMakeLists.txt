cmake_minimum_required(VERSION 3.30.2)
project(command_line_args_parser)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -v")

set(CMAKE_VERBOSE_MAKEFILE ON)

file(GLOB SRC_FILES "src/*.cpp")

find_package(Boost REQUIRED COMPONENTS program_options python)
find_package(Python3 REQUIRED COMPONENTS Development Interpreter)

if (NOT Boost_FOUND)
    message(FATAL_ERROR "Boost was NOT found")
endif()

if (NOT Python3_FOUND)
    message(FATAL_ERROR "Python3 was NOT found")
endif()

set(Boost_USE_MULTITHREADED ON)

add_library(command_line_args_parser SHARED ${SRC_FILES})
target_compile_features(command_line_args_parser PRIVATE cxx_std_20)
target_compile_options(command_line_args_parser PRIVATE -Wall -Wextra -v)
set_target_properties(command_line_args_parser PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if (NOT DEFINED Boost_INCLUDE_DIRS OR "${Boost_INCLUDE_DIRS}" STREQUAL "")
    message(FATAL_ERROR "Boost_INCLUDE_DIRS was NOT set")
endif()

if (NOT DEFINED Python3_INCLUDE_DIRS OR "${Python3_INCLUDE_DIRS}" STREQUAL "")
    message(FATAL_ERROR "Python3_INCLUDE_DIRS was NOT set")
endif()

target_include_directories(
    command_line_args_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${Boost_INCLUDE_DIRS}
    ${Python3_INCLUDE_DIRS}
)

target_link_libraries(
    command_line_args_parser PRIVATE
    Boost::program_options
    Boost::python
)
