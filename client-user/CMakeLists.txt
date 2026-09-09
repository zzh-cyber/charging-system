# 充电用户端（Qt Widgets GUI）

find_package(Qt6 REQUIRED COMPONENTS WebEngineWidgets)

add_executable(charging-user
    main.cpp

    loginwindow.cpp
    loginwindow.h

    mainwindow.cpp
    mainwindow.h

    stationlistpage.cpp
    stationlistpage.h

    stationcardwidget.cpp
    stationcardwidget.h

    pilelistpage.cpp
    pilelistpage.h

    chargepage.cpp
    chargepage.h

    profilepage.cpp
    profilepage.h

    locationmanager.cpp
    locationmanager.h

    navigationpage.cpp
    navigationpage.h

    routeplanner.cpp
    routeplanner.h
    routeresult.h

    amapwidget.cpp
    amapwidget.h

    amapconfig.cpp
    amapconfig.h

    routerequest.h
    appmessagebox.h
    uitheme.h
    mainwindowstyle.h
    windowhelper.h

    fonts.qrc
    uiassets.qrc
    mapresources.qrc
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
        Qt6::WebEngineWidgets
)
