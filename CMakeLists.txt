cmake_minimum_required(VERSION 3.20)

project(nes VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NES_BUILD_TESTS "Build the emulator test suite" ON)
option(NES_BUILD_SDL "Build the SDL2 frontend when SDL2 is available" ON)

add_library(nes_core
    src/cpu.cpp
    src/cartridge.cpp
    src/mapper.cpp
)

target_include_directories(nes_core PUBLIC include)
target_compile_options(nes_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->
)

add_executable(nes_cpu_functional
    tools/cpu_functional_runner.cpp
)
target_link_libraries(nes_cpu_functional PRIVATE nes_core)

if(NES_BUILD_TESTS)
    enable_testing()
    add_executable(nes_tests
        tests/cpu_tests.cpp
        tests/cpu_addressing_tests.cpp
        tests/cpu_alu_tests.cpp
        tests/cpu_mutation_tests.cpp
        tests/cpu_control_tests.cpp
        tests/cartridge_tests.cpp
    )
    target_link_libraries(nes_tests PRIVATE nes_core)
    add_test(NAME nes_tests COMMAND nes_tests)
endif()
