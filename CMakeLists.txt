cmake_minimum_required(VERSION 3.22)
project(QK-LP100A VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum macOS version" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# macOS app bundle
set(MACOSX_BUNDLE_BUNDLE_NAME "QK-LP100A")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.ai5qk.qk-lp100a")
set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort Network Charts)

set(CORE_SOURCES
    src/core/LP100AProtocol.cpp
    src/core/SerialTransport.cpp
    src/core/TcpTransport.cpp
    src/core/HamlibRig.cpp
)

set(CORE_HEADERS
    src/core/ITransport.h
    src/core/LP100AProtocol.h
    src/core/SerialTransport.h
    src/core/TcpTransport.h
    src/core/HamlibRig.h
)
# Note: HamlibClient.h/.cpp removed — project uses direct Hamlib API exclusively

set(VCP_SOURCES
    src/vcp/VCPMainWindow.cpp
    src/vcp/PowerGauge.cpp
    src/vcp/SWRGauge.cpp
    src/vcp/ConnectionDialog.cpp
)

set(VCP_HEADERS
    src/vcp/VCPMainWindow.h
    src/vcp/PowerGauge.h
    src/vcp/SWRGauge.h
    src/vcp/ConnectionDialog.h
)

set(PLOT_SOURCES
    src/plot/PlotWidget.cpp
    src/plot/SweepData.cpp
    src/plot/SweepEngine.cpp
    src/plot/SmithChartWidget.cpp
)

set(PLOT_HEADERS
    src/plot/PlotWidget.h
    src/plot/SweepData.h
    src/plot/SweepEngine.h
    src/plot/SmithChartWidget.h
)

set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/src/resources/icons/AppIcon.icns")
set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")

qt_add_executable(QK-LP100A MACOSX_BUNDLE
    src/main.cpp
    ${CORE_SOURCES}
    ${CORE_HEADERS}
    ${VCP_SOURCES}
    ${VCP_HEADERS}
    ${PLOT_SOURCES}
    ${PLOT_HEADERS}
    ${APP_ICON_MACOS}
)

set_target_properties(QK-LP100A PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/src/resources/Info.plist.in"
)
target_include_directories(QK-LP100A PRIVATE src /opt/homebrew/include)
# Ad-hoc sign after build so macOS allows network access
add_custom_command(TARGET QK-LP100A POST_BUILD
    COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:QK-LP100A>"
    COMMENT "Ad-hoc signing QK-LP100A.app"
)

target_link_libraries(QK-LP100A PRIVATE
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Network
    Qt6::Charts
    /opt/homebrew/lib/libhamlib.dylib
)
