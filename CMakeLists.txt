cmake_minimum_required(VERSION 3.21)
project(WorldGenModule LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 使用方式：
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Concurrent)

# QuaZip from vcpkg installs the generic CONFIG package; the Qt6-specific name is not used here.
find_package(QuaZip-Qt6 REQUIRED)
find_package(BZip2 REQUIRED)
find_package(ZLIB REQUIRED)

file(GLOB_RECURSE WORLDGEN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)
file(GLOB_RECURSE WORLDGEN_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/include/*.h
)

# Windows GUI app: prevents a console window from appearing alongside the Qt UI.
# Without WIN32, CMake defaults to the console subsystem on MSVC and shows a terminal.
add_executable(WorldGenModule WIN32
    ${WORLDGEN_SOURCES}
    ${WORLDGEN_HEADERS}
)

target_include_directories(WorldGenModule PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/ui
)

target_link_libraries(WorldGenModule PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    QuaZip::QuaZip
    BZip2::BZip2
    ZLIB::ZLIB
)
