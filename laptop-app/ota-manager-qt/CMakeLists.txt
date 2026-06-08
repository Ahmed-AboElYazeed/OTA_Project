cmake_minimum_required(VERSION 3.16)
project(OtaManager VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

add_executable(OtaManager
    main.cpp
    MainWindow.cpp
    MainWindow.hpp
    QnxStatusClient.cpp
    QnxStatusClient.hpp
)

target_link_libraries(OtaManager PRIVATE
    Qt6::Core Qt6::Widgets Qt6::Network)
