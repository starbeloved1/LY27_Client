add_library(ly27_commands STATIC
    include/CommandTypes.hpp
    include/CommandDispatcher.hpp
    src/CommandDispatcher.cpp
    src/CommandRequest.cpp
)
target_include_directories(ly27_commands PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ly27_commands PUBLIC ly27_state ly27_mqtt)
