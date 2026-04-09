cmake_minimum_required(VERSION 3.16)
project(VC3DProfileViewer VERSION 2.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Qt: try Qt6 first, fall back to Qt5 ──────────────────────────────────────
find_package(Qt6 COMPONENTS Core Gui Widgets Charts Network Svg QUIET)
if(Qt6_FOUND)
    message(STATUS "Using Qt6")
    set(QT_TARGET Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Charts Qt6::Network Qt6::Svg)
    set(QT_PREFIX  Qt6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Charts Network Svg)
    message(STATUS "Using Qt5")
    set(QT_TARGET Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Charts Qt5::Network Qt5::Svg)
    set(QT_PREFIX  Qt5)
endif()

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/profilewidget.cpp
    src/sensorworker.cpp
    src/vcprotocol.cpp
    src/jsonplayer.cpp
    src/measurementlogger.cpp   # CSV measurement log
)

set(HEADERS
    include/types.h               # Shared types (ProfilePoint, RoiRect) – no Win headers
    include/mainwindow.h
    include/profilewidget.h
    include/sensorworker.h
    include/vcprotocol.h
    include/jsonplayer.h
    include/measurementlogger.h   # CSV measurement log
)

# ── Executable ────────────────────────────────────────────────────────────────
if(WIN32)
    add_executable(VC3DProfileViewer WIN32 ${SOURCES} ${HEADERS})
else()
    add_executable(VC3DProfileViewer ${SOURCES} ${HEADERS})
endif()

target_include_directories(VC3DProfileViewer PRIVATE include)
target_link_libraries(VC3DProfileViewer PRIVATE ${QT_TARGET})

# ── Embed resources (logo SVG) ────────────────────────────────────────────────
if(COMMAND qt_add_resources)
    # Qt6 style
    qt_add_resources(VC3DProfileViewer "app_resources"
        PREFIX "/images"
        BASE "${CMAKE_SOURCE_DIR}/resources"
        FILES resources/logo_notavis.svg
    )
else()
    # Qt5 style
    qt5_add_resources(RESOURCE_FILES resources/resources.qrc)
    target_sources(VC3DProfileViewer PRIVATE ${RESOURCE_FILES})
endif()

# ── Output directory ──────────────────────────────────────────────────────────
# Release build goes to bin/Release – you then run windeployqt there
set_target_properties(VC3DProfileViewer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/bin"
)

# ── Post-build: create Data, Devices and TestData folders next to EXE ────────
add_custom_command(TARGET VC3DProfileViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:VC3DProfileViewer>/Data"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:VC3DProfileViewer>/Devices"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:VC3DProfileViewer>/TestData"
    # Copy the 20 bundled test profiles next to the EXE
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/tests/data"
        "$<TARGET_FILE_DIR:VC3DProfileViewer>/TestData"
    # Copy SVG next to EXE as filesystem fallback for watermark
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:VC3DProfileViewer>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/resources/logo_notavis.svg"
        "$<TARGET_FILE_DIR:VC3DProfileViewer>/resources/logo_notavis.svg"
    COMMENT "Creating portable folder structure + copying TestData + resources"
)
