# 公共层：协议定义 + 客户端网络基类
add_library(common STATIC
    netclient.cpp
    netclient.h
    protocol.h
)

set_target_properties(common PROPERTIES AUTOMOC ON)

target_link_libraries(common
    PUBLIC
        Qt6::Core
        Qt6::Network
)

# 让依赖 common 的目标能直接 #include "protocol.h" / "netclient.h"
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
