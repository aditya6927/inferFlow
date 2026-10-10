cmake_minimum_required(VERSION 3.20)

project(infsim
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(infsim
    src/main.cpp
    src/simulator.cpp
    src/metrics.cpp
)

target_include_directories(infsim
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)