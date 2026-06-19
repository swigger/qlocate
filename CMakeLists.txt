cmake_minimum_required(VERSION 3.16)
project(qlocate LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(qlocate
    src/main.cpp
    src/config.cpp
    src/indexer.cpp
    src/searcher.cpp
    src/mainwindow.cpp
)

target_include_directories(qlocate PRIVATE third_party src)
target_link_libraries(qlocate PRIVATE Qt5::Widgets)

if(APPLE)
    target_link_libraries(qlocate PRIVATE "-framework CoreFoundation")
endif()
