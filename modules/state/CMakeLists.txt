add_library(ly27_state STATIC
    include/StateStore.hpp
    src/StateStore.cpp
)
target_include_directories(ly27_state PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ly27_state PUBLIC ly27_core ly27_protocol)
