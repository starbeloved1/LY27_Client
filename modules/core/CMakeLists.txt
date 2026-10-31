add_library(ly27_core STATIC
    TerminalIdentity.hpp
    TerminalIdentity.cpp
)
target_include_directories(ly27_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ly27_core PUBLIC Qt6::Core)
