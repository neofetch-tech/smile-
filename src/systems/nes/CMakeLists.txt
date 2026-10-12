cmake_minimum_required(VERSION 3.22)
project(smile_nes LANGUAGES CXX)

add_library(smile_nes
    NesSystem.cpp
    bus/NesBus.cpp
    cartridge/NesCartridge.cpp
    cpu/NesCPU_base.cpp
    cpu/NesCPU_exec.cpp
    ppu/NesPPU.cpp
    ppu/NesPPU_render.cpp
)

target_include_directories(smile_nes PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(smile_nes PRIVATE smile_core)
target_compile_features(smile_nes PUBLIC cxx_std_23)
