cmake_minimum_required(VERSION 3.16)

project(qt_widgets_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 目标 BitDev 环境：Qt 6.2.4。这里显式列出后续项目会用到的模块进行验证。
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network SerialPort Sql)

add_executable(qt_widgets_test main.cpp)

target_link_libraries(qt_widgets_test
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::SerialPort
        Qt6::Sql
)
