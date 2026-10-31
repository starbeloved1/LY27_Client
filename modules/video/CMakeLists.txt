find_package(Qt6 6.8 REQUIRED COMPONENTS Network Gui)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec>=58 libavutil>=56 libswscale>=5)
add_library(ly27_video STATIC
    include/VideoTypes.hpp
    include/VideoAssembler.hpp src/VideoAssembler.cpp
    include/HevcDecoder.hpp src/HevcDecoder.cpp
    include/VideoReceiver.hpp src/VideoReceiver.cpp
)
target_include_directories(ly27_video PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ly27_video PUBLIC Qt6::Core Qt6::Gui Qt6::Network PRIVATE PkgConfig::FFMPEG)
