add_library(ly27_config STATIC
    include/TerminalConfig.hpp
    src/TerminalConfig.cpp
)
target_include_directories(ly27_config PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ly27_config PUBLIC ly27_core)
