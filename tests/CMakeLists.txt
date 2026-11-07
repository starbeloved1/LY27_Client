foreach(name config runtime cli protocol state state_models)
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

add_executable(test_commands test_commands.cpp)
target_link_libraries(test_commands PRIVATE ly27_app Qt6::Test Qt6::Network)
target_compile_definitions(test_commands PRIVATE
    LY27_BROKER_PATH="${MOSQUITTO_EXECUTABLE}"
    LY27_CLI_PATH="$<TARGET_FILE:ly27_cli>"
    LY27_EXAMPLE_CONFIG="${PROJECT_SOURCE_DIR}/config/terminal.example.json")
add_dependencies(test_commands ly27_cli)
add_test(NAME commands COMMAND test_commands)
set_tests_properties(commands PROPERTIES TIMEOUT 120)

add_executable(test_video test_video.cpp)
target_link_libraries(test_video PRIVATE ly27_app Qt6::Test Qt6::Network)
target_compile_definitions(test_video PRIVATE
    LY27_VIDEO_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/video.hevc"
    LY27_CLI_PATH="$<TARGET_FILE:ly27_cli>"
    LY27_EXAMPLE_CONFIG="${PROJECT_SOURCE_DIR}/config/terminal.example.json")
add_dependencies(test_video ly27_cli)
add_test(NAME video COMMAND test_video)
set_tests_properties(video PROPERTIES TIMEOUT 30)

add_executable(test_reconfiguration test_reconfiguration.cpp)
target_link_libraries(test_reconfiguration PRIVATE ly27_app Qt6::Test Qt6::Network)
target_compile_definitions(test_reconfiguration PRIVATE LY27_CLI_PATH="$<TARGET_FILE:ly27_cli>")
add_dependencies(test_reconfiguration ly27_cli)
add_test(NAME reconfiguration COMMAND test_reconfiguration)
set_tests_properties(reconfiguration PROPERTIES TIMEOUT 40)

if(LY27_BUILD_DESKTOP)
    add_executable(test_match_notices test_match_notices.cpp)
    target_link_libraries(test_match_notices PRIVATE ly27_ui Qt6::Test)
    add_test(NAME match_notices COMMAND test_match_notices)
    # 通告动效测试等待完整出入场及补位过程。
    set_tests_properties(match_notices PROPERTIES TIMEOUT 35
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(test_buffs test_buffs.cpp)
    target_link_libraries(test_buffs PRIVATE ly27_ui Qt6::Test)
    add_test(NAME buffs COMMAND test_buffs)
    set_tests_properties(buffs PROPERTIES TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(test_tactical_map test_tactical_map.cpp)
    target_link_libraries(test_tactical_map PRIVATE ly27_ui Qt6::Test Qt6::Network)
    target_compile_definitions(test_tactical_map PRIVATE LY27_BROKER_PATH="${MOSQUITTO_EXECUTABLE}")
    add_test(NAME tactical_map COMMAND test_tactical_map)
    set_tests_properties(tactical_map PROPERTIES TIMEOUT 45
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(test_territories test_territories.cpp)
    target_link_libraries(test_territories PRIVATE ly27_ui Qt6::Test)
    add_test(NAME territories COMMAND test_territories)
    set_tests_properties(territories PROPERTIES TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(test_injury test_injury.cpp)
    target_link_libraries(test_injury PRIVATE ly27_ui Qt6::Test)
    add_test(NAME injury COMMAND test_injury)
    set_tests_properties(injury PROPERTIES TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(test_events test_events.cpp)
    target_link_libraries(test_events PRIVATE ly27_ui Qt6::Test)
    add_test(NAME events COMMAND test_events)
    set_tests_properties(events PROPERTIES TIMEOUT 15)
    add_executable(test_desktop test_desktop.cpp)
    target_link_libraries(test_desktop PRIVATE ly27_ui Qt6::Test)
    add_test(NAME desktop COMMAND test_desktop)
    set_tests_properties(desktop PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_executable(test_presentation test_presentation.cpp)
    target_link_libraries(test_presentation PRIVATE ly27_ui Qt6::Test)
    add_test(NAME presentation COMMAND test_presentation)
    set_tests_properties(presentation PROPERTIES TIMEOUT 60
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
