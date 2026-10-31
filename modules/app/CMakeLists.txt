add_library(ly27_app STATIC
    include/RuntimeLogger.hpp
    include/TerminalRuntime.hpp
    src/RuntimeLogger.cpp
    src/TerminalRuntime.cpp
)
target_include_directories(ly27_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ly27_app PUBLIC ly27_config ly27_protocol ly27_mqtt ly27_state ly27_commands ly27_video)
target_compile_definitions(ly27_app PUBLIC LY27_VERSION="${PROJECT_VERSION}")
