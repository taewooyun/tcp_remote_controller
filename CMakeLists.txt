# 프로젝트 메타 정보
# =========================
cmake_minimum_required(VERSION 3.16)
project(tcp_remote_controller C)

# 언어 표준
# =========================
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 출력 디렉토리 설정
# =========================
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/exec)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/lib)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/client)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/server)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/server/data)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/exec)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/lib)

# 공유 라이브러리
# =========================
set(SENSOR_SRCS
    code/server/sensors/led.c
    code/server/sensors/cds.c
    code/server/sensors/buzzer.c
    code/server/sensors/fnd.c
    code/server/sensors/camera.c
)
add_library(sensors SHARED
    ${SENSOR_SRCS}
)
target_include_directories(sensors PUBLIC
    ${CMAKE_SOURCE_DIR}/code/server/include
    ${CMAKE_SOURCE_DIR}/code/server/sensors
)
target_link_libraries(sensors
    wiringPi
    pthread
)

# 실행파일_서버
# =========================
add_executable(server
    code/server/src/main.c
    code/server/src/server.c
    code/server/src/client_handler.c
    code/server/src/command_queue.c
    code/server/src/device_worker.c
    code/server/src/file_transfer.c
)
target_link_libraries(server
    sensors
    pthread
)
set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/server
    BUILD_RPATH "$ORIGIN/../lib"
)

# 실행파일_서버
# =========================
add_executable(client
    code/client/client.c
)
set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/client
)

# 컴파일 옵션
# =========================
foreach(target sensors server client)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
    )
endforeach()
