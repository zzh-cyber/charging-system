cmake_minimum_required(VERSION 3.16)

project(charging_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 目标环境：Ubuntu 22.04 + Qt 6.2.4（BitDev 对齐），不使用 6.2.4 之后的新 API。
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql)

add_subdirectory(common)        # 公共层：协议 + 网络基类
add_subdirectory(server)        # 服务器：TCP + 多线程 + MySQL
add_subdirectory(client-user)   # 充电用户端
add_subdirectory(client-admin)  # PC 管理端
