add_library(smile_core
    include/core/ISystem.hpp
    include/core/Settings.hpp
)

target_include_directories(smile_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(smile_core PUBLIC cxx_std_23)
