cmake_minimum_required(VERSION 3.16)
project(auction)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(src)

# Core library
add_library(auction_core STATIC 
    src/book/cas-book.cpp 
    src/parse/parser.cpp
)

# Main executable
add_executable(auction src/main.cpp)
target_link_libraries(auction PRIVATE auction_core)

if(MSVC)
    target_compile_options(auction PRIVATE /O2)
    target_compile_options(auction_core PRIVATE /O2)
else()
    target_compile_options(auction PRIVATE -O3 -march=native)
    target_compile_options(auction_core PRIVATE -O3 -march=native)
endif()

# Enable testing
enable_testing()

# Test executable
add_executable(run_tests tests/test_main.cpp)
target_link_libraries(run_tests PRIVATE auction_core)

# Register test with CTest
add_test(NAME AllTests 
         COMMAND run_tests 
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
