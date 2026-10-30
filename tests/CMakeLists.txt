foreach(name config runtime cli)
    add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE ly27_core Qt6::Test)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()

target_compile_definitions(test_config PRIVATE
    LY27_EXAMPLE_CONFIG="${PROJECT_SOURCE_DIR}/config/terminal.example.json")
target_compile_definitions(test_cli PRIVATE
    LY27_CLI_PATH="$<TARGET_FILE:ly27_cli>"
    LY27_EXAMPLE_CONFIG="${PROJECT_SOURCE_DIR}/config/terminal.example.json")
add_dependencies(test_cli ly27_cli)
set_tests_properties(cli PROPERTIES TIMEOUT 30)
