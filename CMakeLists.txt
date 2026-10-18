cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(VirtualIndustrialIoT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PahoMqttCpp CONFIG REQUIRED)

file(GLOB_RECURSE CORE_SOURCES
    "${CMAKE_SOURCE_DIR}/src/automation/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/config/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/devices/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/events/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/health/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/mqtt/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/storage/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/telemetry/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/factory/*.cpp"
)

add_executable(virtual-industrial-iot
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CORE_SOURCES}
)

target_include_directories(virtual-industrial-iot
    PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(virtual-industrial-iot
    PRIVATE
        PahoMqttCpp::paho-mqttpp3
)

add_executable(industrial-iot-tests
    ${CMAKE_SOURCE_DIR}/tests/test_suite.cpp
    ${CORE_SOURCES}
)

target_include_directories(industrial-iot-tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(industrial-iot-tests
    PRIVATE
        PahoMqttCpp::paho-mqttpp3
)