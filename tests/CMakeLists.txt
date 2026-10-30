foreach(name config runtime cli protocol)
    add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE ly27_app Qt6::Test)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()

target_compile_definitions(test_config PRIVATE
    LY27_EXAMPLE_CONFIG="${PROJECT_SOURCE_DIR}/config/terminal.example.json")
target_compile_definitions(test_cli PRIVATE
    LY27_CLI_PATH="$<TARGET_FILE:ly27_cli>"
    LY27_EXAMPLE_CONFIG="${PROJECT_SOURCE_DIR}/config/terminal.example.json")
add_dependencies(test_cli ly27_cli)
set_tests_properties(cli PROPERTIES TIMEOUT 30)

# 集成测试只使用本机独立 Broker，不访问真实赛事网络。
find_package(Qt6 6.8 REQUIRED COMPONENTS Network)
find_program(MOSQUITTO_EXECUTABLE mosquitto REQUIRED)
add_executable(test_mqtt test_mqtt.cpp)
target_link_libraries(test_mqtt PRIVATE ly27_app Qt6::Test Qt6::Network)
target_compile_definitions(test_mqtt PRIVATE
    LY27_BROKER_PATH="${MOSQUITTO_EXECUTABLE}"
    LY27_CLI_PATH="$<TARGET_FILE:ly27_cli>"
    LY27_EXAMPLE_CONFIG="${PROJECT_SOURCE_DIR}/config/terminal.example.json")
add_dependencies(test_mqtt ly27_cli)
add_test(NAME mqtt COMMAND test_mqtt)
set_tests_properties(mqtt PROPERTIES TIMEOUT 120)
