# 充电用户端（Qt Widgets GUI）
add_executable(charging-user
    main.cpp
    loginwindow.cpp
    loginwindow.h
    mainwindow.cpp
    mainwindow.h
    stationlistpage.cpp
    stationlistpage.h
    pilelistpage.cpp
    pilelistpage.h
    chargepage.cpp
    chargepage.h
    profilepage.cpp
    profilepage.h
    fonts.qrc
    locationmanager.cpp
    locationmanager.h

)

set_target_properties(charging-user PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_link_libraries(charging-user
    PRIVATE
        common
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
)
