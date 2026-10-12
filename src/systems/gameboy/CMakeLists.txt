cmake_minimum_required(VERSION 3.22)
project(smile_gameboy LANGUAGES CXX)

add_library(smile_gameboy
    GameBoySystem.cpp
    bus/MemoryBus.cpp
    bus/MemoryBus_state.cpp
    cartridge/Cartridge.cpp
    apu/APU.cpp
    cpu/CPU_base.cpp
    cpu/CPU_step.cpp
    cpu/CPU_state.cpp
    ppu/PPU.cpp
    ppu/PPU_state.cpp
    timer/Timer.cpp
    timer/Timer_state.cpp
)

target_include_directories(smile_gameboy PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(smile_gameboy PRIVATE smile_core)
target_compile_features(smile_gameboy PUBLIC cxx_std_23)
