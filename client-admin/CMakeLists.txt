# PC 管理端（Qt Widgets GUI）
add_executable(charging-admin
    main.cpp
    adminloginwindow.cpp
    adminloginwindow.h
)

set_target_properties(charging-admin PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_link_libraries(charging-admin
    PRIVATE
        common
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
)
