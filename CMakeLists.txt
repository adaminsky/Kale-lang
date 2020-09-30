cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

project(Kaleidoscope)

set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)
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
include_directories(include)
add_compile_options(-fno-rtti)

# Now build our tools
add_library(lexer_lib src/lexer.cc)
add_library(parser_lib src/parser.cc)
add_library(ast_lib src/ast.cc)
target_link_libraries(parser_lib lexer_lib ast_lib)
add_executable(Kale src/kale_main.cc)
target_link_libraries(Kale parser_lib)

# Link against LLVM libraries
target_link_libraries(Kale ${LLVM_AVAILABLE_LIBS} -lz -lrt -ldl -ltinfo -lpthread -lm)
