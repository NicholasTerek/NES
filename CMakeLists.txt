cmake_minimum_required(VERSION 3.20)

project(nes VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NES_BUILD_TESTS "Build the emulator test suite" ON)
option(NES_BUILD_SDL "Build the SDL2 frontend when SDL2 is available" ON)
option(NES_BUILD_WEB "Build the WebAssembly frontend with Emscripten" OFF)

add_library(nes_core
    src/apu.cpp
    src/bus.cpp
    src/cpu.cpp
    src/emulator.cpp
    src/cartridge.cpp
    src/mapper.cpp
    src/ppu.cpp
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

add_executable(nes_runner
    tools/nes_runner.cpp
)
target_link_libraries(nes_runner PRIVATE nes_core)

add_executable(nes_conformance
    tools/nes_conformance.cpp
)
target_link_libraries(nes_conformance PRIVATE nes_core)

if(NES_BUILD_SDL)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        add_executable(nes_sdl
            tools/nes_sdl.cpp
        )
        target_link_libraries(nes_sdl PRIVATE nes_core)
        if(TARGET SDL2::SDL2)
            target_link_libraries(nes_sdl PRIVATE SDL2::SDL2)
        else()
            target_include_directories(nes_sdl PRIVATE ${SDL2_INCLUDE_DIRS})
            target_link_libraries(nes_sdl PRIVATE ${SDL2_LIBRARIES})
        endif()
    endif()
endif()

if(NES_BUILD_WEB)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "NES_BUILD_WEB requires Emscripten (configure with emcmake).")
    endif()

    add_executable(nes_web web/wasm/nes_web.cpp)
    target_link_libraries(nes_web PRIVATE nes_core)
    set_target_properties(nes_web PROPERTIES
        OUTPUT_NAME "nes"
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web"
    )
    target_link_options(nes_web PRIVATE
        "-sMODULARIZE=1"
        "-sEXPORT_ES6=1"
        "-sEXPORT_NAME=createNesModule"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_RUNTIME_METHODS=['ccall']"
        "-sEXPORTED_FUNCTIONS=['_malloc','_free']"
    )
    add_custom_command(TARGET nes_web POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/web/index.html" "${CMAKE_BINARY_DIR}/web/index.html"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/web/main.js" "${CMAKE_BINARY_DIR}/web/main.js"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/web/scene.js" "${CMAKE_BINARY_DIR}/web/scene.js"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/web/style.css" "${CMAKE_BINARY_DIR}/web/style.css"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/web/assets" "${CMAKE_BINARY_DIR}/web/assets"
    )
endif()

if(NES_BUILD_TESTS)
    enable_testing()
    add_executable(nes_tests
        tests/cpu_tests.cpp
        tests/cpu_addressing_tests.cpp
        tests/cpu_alu_tests.cpp
        tests/cpu_mutation_tests.cpp
        tests/cpu_control_tests.cpp
        tests/cpu_unofficial_tests.cpp
        tests/cartridge_tests.cpp
        tests/bus_tests.cpp
        tests/ppu_memory_tests.cpp
        tests/ppu_register_tests.cpp
        tests/ppu_timing_tests.cpp
        tests/ppu_background_tests.cpp
        tests/ppu_sprite_tests.cpp
        tests/apu_tests.cpp
        tests/emulator_tests.cpp
        tests/system_tests.cpp
    )
    target_link_libraries(nes_tests PRIVATE nes_core)
    add_test(NAME nes_tests COMMAND nes_tests)
endif()
