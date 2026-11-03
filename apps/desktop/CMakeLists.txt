add_executable(ly27_desktop src/main.cpp)
target_link_libraries(ly27_desktop PRIVATE ly27_ui)
set_target_properties(ly27_desktop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
