# 服务器端：TCP + 多线程 + MySQL
add_executable(charging-server
    main.cpp
    tcpserver.cpp
    tcpserver.h
    clienthandler.cpp
    clienthandler.h
    database.cpp
    database.h
    config.h
    schema.qrc
)

set_target_properties(charging-server PROPERTIES AUTOMOC ON AUTORCC ON)

target_link_libraries(charging-server
    PRIVATE
        common
        Qt6::Core
        Qt6::Network
        Qt6::Sql
)
