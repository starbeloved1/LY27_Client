cmake_minimum_required(VERSION 3.22)
project(LY27Client VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(ly27_core STATIC
    src/core/TerminalIdentity.cpp
    src/config/TerminalConfig.cpp
    src/app/RuntimeLogger.cpp
    src/app/TerminalRuntime.cpp
)
target_include_directories(ly27_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ly27_core PUBLIC Qt6::Core)
target_compile_definitions(ly27_core PUBLIC LY27_VERSION="${PROJECT_VERSION}")
if(MSVC)
    target_compile_options(ly27_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(ly27_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ly27_cli apps/cli/main.cpp)
target_link_libraries(ly27_cli PRIVATE ly27_core)
set_target_properties(ly27_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
# Only copy the example. User configuration is never generated over or modified.
configure_file(config/terminal.example.json
    "${CMAKE_BINARY_DIR}/bin/config/terminal.example.json" COPYONLY)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
