add_executable(ly27_cli src/main.cpp)
target_link_libraries(ly27_cli PRIVATE ly27_app)
set_target_properties(ly27_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
