cmake_minimum_required(VERSION 3.16)

project(imu_service VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(PostgreSQL REQUIRED)

add_executable(imu_service
    main.cpp
    src/config/ConfigLoader.cpp
    src/driver/MPU6050.cpp
    src/algorithm/MadgwickAHRS.cpp
    src/database/PostgreSQLManager.cpp
    src/service/IMUService.cpp
)

target_include_directories(imu_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PostgreSQL_INCLUDE_DIRS}
)

target_link_libraries(imu_service PRIVATE
    ${PostgreSQL_LIBRARIES}
    Threads::Threads
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imu_service PRIVATE -Wall -Wextra -Wpedantic)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/config.yaml
    ${CMAKE_CURRENT_BINARY_DIR}/config/config.yaml
    COPYONLY
)
