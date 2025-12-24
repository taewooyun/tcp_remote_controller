cmake_minimum_required(VERSION 3.16)

project(tcp_remote_controller C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =========================
# 출력 디렉토리
# =========================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/exec)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

# =========================
# Sensors Shared Library
# =========================
file(GLOB SENSOR_SRCS
    code/server/sensors/*.c
)

add_library(sensors SHARED
    ${SENSOR_SRCS}
)

target_include_directories(sensors PUBLIC
    ${CMAKE_SOURCE_DIR}/code/server/sensors
)

# libsensors 의존성
target_link_libraries(sensors
    wiringPi
    pthread
)

# =========================
# Server Executable
# =========================
add_executable(server
    code/server/server.c
)

target_link_libraries(server
    sensors
    pthread
)

# rpath
set_target_properties(server PROPERTIES
    BUILD_RPATH "$ORIGIN/../lib"
)

# =========================
# Client Executable
# =========================
add_executable(client
    code/client/client.c
)

# =========================
# 컴파일 옵션
# =========================
foreach(target sensors server client)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
    )
endforeach()
