cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# Project Configuration
# ==============================================================================
project(keel
    VERSION 0.1.0
    DESCRIPTION "A low-level storage engine and distributed database written in C++20"
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Force out-of-source builds to prevent cluttering the source directory
if(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "In-source builds are forbidden. Please create a build directory (e.g., 'mkdir build && cd build') and run CMake from there.")
endif()

# Set C++ Standard globally
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for LSP support (clangd)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Debug build if none specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build (Debug, Release, RelWithDebInfo, MinSizeRel)." FORCE)
endif()

# ==============================================================================
# Build Options & Feature Flags
# ==============================================================================
option(KEEL_ENABLE_SANITIZERS "Enable Address and UndefinedBehavior Sanitizers" ON)
option(KEEL_ENABLE_TSAN "Enable ThreadSanitizer (Mutually exclusive with ASan)" OFF)
option(KEEL_ENABLE_TESTS "Build test suite" ON)
option(KEEL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

# ==============================================================================
# Compiler Flags & Systems Mode Config (No Exceptions, No RTTI)
# ==============================================================================
add_library(keel_compiler_flags INTERFACE)

target_compile_options(keel_compiler_flags INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wconversion
        -Wsign-conversion
        -Wold-style-cast
        -Wunused
        # -fno-exceptions      # Disable C++ exceptions for deterministic control
        -fno-rtti            # Disable runtime type identification
        $<$<BOOL:${KEEL_WARNINGS_AS_ERRORS}>:-Werror>
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /permissive-
        /GR-                 # Disable RTTI in MSVC
        $<$<BOOL:${KEEL_WARNINGS_AS_ERRORS}>:/WX>
    >
)

# ==============================================================================
# Sanitizers Setup (ASan, UBSan, TSan)
# ==============================================================================
if(KEEL_ENABLE_SANITIZERS AND KEEL_ENABLE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be enabled simultaneously. Choose either KEEL_ENABLE_SANITIZERS or KEEL_ENABLE_TSAN.")
endif()

if(KEEL_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        message(STATUS "Enabling AddressSanitizer and UndefinedBehaviorSanitizer")
        target_compile_options(keel_compiler_flags INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(keel_compiler_flags INTERFACE -fsanitize=address,undefined)
    endif()
elseif(KEEL_ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        message(STATUS "Enabling ThreadSanitizer")
        target_compile_options(keel_compiler_flags INTERFACE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(keel_compiler_flags INTERFACE -fsanitize=thread)
    endif()
endif()

# ==============================================================================
# Clang-Tidy Integration (Static Analysis)
# ==============================================================================
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY
        ${CLANG_TIDY_EXE};
        -warnings-as-errors=*;
        -checks=-*,bugprone-*,performance-*,portability-*,clang-analyzer-*
    )
else()
    message(STATUS "clang-tidy not found. Static analysis step skipped.")
endif()

# ==============================================================================
# Core Engine Library (Target)
# ==============================================================================
# Add core source files here
add_library(keel_core STATIC
    src/keel.cpp
    src/types.cpp
    src/memtable/skip_list.cpp
    src/memtable/memtable.cpp
    src/wal.cpp
)

target_include_directories(keel_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(keel_core
    PUBLIC
        keel_compiler_flags
)


# Include Catch2 for testing

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.1 # or a later release
)

FetchContent_MakeAvailable(Catch2)

# ==============================================================================
# Main Executable / CLI
# ==============================================================================
add_executable(keeld
    src/main.cpp
)

target_link_libraries(keeld
    PRIVATE
        keel_core
        keel_compiler_flags
)

# ==============================================================================
# Testing Framework (CTest)
# ==============================================================================
if(KEEL_ENABLE_TESTS)
    enable_testing()

    add_executable(keel_tests
        tests/keel_test.cpp
        tests/skip_list_test.cpp
    )

    add_executable(skip_list_tests
        tests/skip_list_test.cpp
    )

    add_executable(wal_tests
        tests/wal_test.cpp
    )

    set(KEEL_TESTS "keel_tests" "skip_list_tests" "wal_tests")

    foreach(test IN LISTS KEEL_TESTS)
        target_link_libraries(${test}
            PRIVATE
            keel_core
            keel_compiler_flags
            Catch2::Catch2WithMain
        )
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
endif()