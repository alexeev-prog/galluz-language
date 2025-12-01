cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    galluzlang
    VERSION 0.1.0
    DESCRIPTION "A Programming Language based on S-expressions in C++ and LLVM 14"
    HOMEPAGE_URL "https://github.com/alexeev-prog/galluz-language"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

find_package(LLVM REQUIRED CONFIG)
find_package(LLD REQUIRED CONFIG)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -finput-charset=UTF-8 -Waddress -O1 -pedantic-errors -Wall -Wextra -Wpedantic -Wcast-align -Wcast-qual -Wconversion -Wctor-dtor-privacy -Wextra-semi -Wfloat-equal -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wsign-conversion")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})

# ---- Declare library ----

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs support core irreader TargetParser Target)

add_library(
    galluzlang_lib OBJECT
    source/tracelogger.cpp
    source/logger.cpp
    source/input_parser.cpp
)
target_link_libraries(galluzlang_lib ${llvm_libs} lldELF)
target_link_libraries(galluzlang_lib
    LLVMPasses
    LLVMX86CodeGen
    LLVMX86Desc
    LLVMX86Info
    LLVMOrcJIT
    LLVMExecutionEngine
    LLVMRuntimeDyld
    LLVMMCJIT
    LLVMTarget
)

target_include_directories(
    galluzlang_lib ${warning_guard}
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(galluzlang_lib PUBLIC cxx_std_17)

# ---- Declare executable ----

add_executable(galluzlang_exe source/main.cpp)
add_executable(galluzlang::exe ALIAS galluzlang_exe)

set_property(TARGET galluzlang_exe PROPERTY OUTPUT_NAME galluzlang)

target_compile_features(galluzlang_exe PRIVATE cxx_std_17)

target_link_libraries(galluzlang_exe PRIVATE galluzlang_lib)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT galluzlang_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of galluzlang"
  )
endif()

include(cmake/dev-mode.cmake)
