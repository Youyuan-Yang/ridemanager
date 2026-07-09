cmake_minimum_required(VERSION 3.21)

project(RideManagerQml VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Sql
    Concurrent
)

qt_standard_project_setup(REQUIRES 6.4)

set(RIDEMANAGER_CPP_FILES
    main.cpp
    DatabaseManager.h
    DatabaseManager.cpp
    RideManagerConfig.h
    RideManagerConfig.cpp
    SensorCatalog.h
    SensorCatalog.cpp
    SafetyDecisionModel.h
    SafetyDecisionModel.cpp
    CameraFindingModel.h
    CameraFindingModel.cpp
    SensorReadingModel.h
    SensorReadingModel.cpp
    AppController.h
    AppController.cpp
)

foreach(RIDEMANAGER_FILE IN LISTS RIDEMANAGER_CPP_FILES)
    if(NOT EXISTS "${RIDEMANAGER_FILE}")
        message(FATAL_ERROR
            "Missing source file: ${RIDEMANAGER_FILE}\n"
            "Please configure from the RideManagerQml project root, for example:\n"
            "  cmake -S . -B build")
    endif()
endforeach()

qt_add_executable(RideManagerQml
    ${RIDEMANAGER_CPP_FILES}
)

set(RIDEMANAGER_QML_FILES
    Main.qml
    qml/components/CameraPanel.qml
    qml/components/RiskCard.qml
    qml/components/SideNav.qml
    qml/components/StatTile.qml
    qml/components/SensorTrend.qml
    qml/components/DataTable.qml
    qml/components/StatusBar.qml
)

foreach(RIDEMANAGER_FILE IN LISTS RIDEMANAGER_QML_FILES)
    if(NOT EXISTS "${RIDEMANAGER_FILE}")
        message(FATAL_ERROR
            "Missing QML file: ${RIDEMANAGER_FILE}\n"
            "Please configure from the RideManagerQml project root, for example:\n"
            "  cmake -S . -B build")
    endif()
endforeach()

qt_add_qml_module(RideManagerQml
    URI RideManager
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        ${RIDEMANAGER_QML_FILES}
)

target_link_libraries(RideManagerQml
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Sql
        Qt6::Concurrent
)

set_target_properties(RideManagerQml PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
