add_library(ly27_core STATIC
    include/TerminalIdentity.hpp
    src/TerminalIdentity.cpp
)
target_include_directories(ly27_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ly27_core PUBLIC Qt6::Core)
