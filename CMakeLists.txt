cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_COMPILER /usr/bin/gcc)

project(Kaleidoscope)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set your project compile flags.
# E.g. if using the C++ header files
# you will need to enable C++11 support
# for your compiler.
set(CMAKE_BUILD_TYPE Debug)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Now build our tools
add_library(lexer_lib src/lexer.cc)
add_library(parser_lib src/parser.cc)
add_library(ast_lib src/ast.cc)
target_link_libraries(parser_lib lexer_lib ast_lib)
add_executable(Kale src/kale_main.cc)
target_link_libraries(Kale parser_lib)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader)

# Link against LLVM libraries
target_link_libraries(Kale ${llvm_libs})
