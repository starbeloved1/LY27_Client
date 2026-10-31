find_package(Protobuf 3.15 REQUIRED)

# 生成的 C++ 协议文件只保存在本模块的构建目录。
protobuf_generate_cpp(PROTOCOL_SOURCES PROTOCOL_HEADERS rm2027.proto)
add_library(ly27_protocol STATIC
    MessageCodec.hpp
    MessageCodec.cpp
    ${PROTOCOL_SOURCES}
    ${PROTOCOL_HEADERS}
)
target_include_directories(ly27_protocol PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(ly27_protocol PUBLIC Qt6::Core protobuf::libprotobuf)
qt_add_resources(ly27_protocol protocol_resources
    PREFIX "/ly27/protocol"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES messages.json
)
