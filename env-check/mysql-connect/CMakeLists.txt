cmake_minimum_required(VERSION 3.16)

project(mysql_connect_check LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql)

add_executable(mysql_connect_check main.cpp)

target_link_libraries(mysql_connect_check
    PRIVATE
        Qt6::Core
        Qt6::Sql
)
