add_executable(smile
    main.cpp
    MainWindow.hpp
    MainWindow.cpp
    EmulatorViewport.hpp
    EmulatorViewport.cpp
    AudioPlayer.hpp
    AudioPlayer.cpp
)

# Application icon/logo: compiled in only when assets/logo.png exists,
# so a missing logo never breaks the build.
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/logo.png")
    target_sources(smile PRIVATE "${CMAKE_SOURCE_DIR}/assets/app.qrc")
    target_compile_definitions(smile PRIVATE SMILE_HAS_LOGO=1)
endif()

target_link_libraries(smile PRIVATE
        smile_core
        smile_gameboy
        smile_nes
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::OpenGLWidgets
)

target_compile_features(smile PRIVATE cxx_std_23)
