cmake_minimum_required(VERSION 3.22)
project(LY27Client VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

# 根构建只组织依赖顺序，各模块自行声明源码、头文件路径和链接依赖。
add_subdirectory(modules/core)
add_subdirectory(modules/config)
add_subdirectory(modules/protocol)
add_subdirectory(modules/mqtt)
add_subdirectory(modules/state)
add_subdirectory(modules/app)
add_subdirectory(apps/cli)

foreach(target ly27_core ly27_config ly27_protocol ly27_mqtt ly27_state ly27_app ly27_cli)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
# 只复制示例配置，用户配置不会被生成或覆盖。
configure_file(config/terminal.example.json
    "${CMAKE_BINARY_DIR}/bin/config/terminal.example.json" COPYONLY)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
