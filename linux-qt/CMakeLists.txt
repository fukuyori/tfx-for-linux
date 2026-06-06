cmake_minimum_required(VERSION 3.20)

project(tfx-for-linux VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(tfx
    src/AppConfig.cpp
    src/AppConfig.h
    src/FilePane.cpp
    src/FilePane.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/PreviewPane.cpp
    src/PreviewPane.h
    src/TerminalPane.cpp
    src/TerminalPane.h
    src/UiText.cpp
    src/UiText.h
    src/main.cpp
)

target_link_libraries(tfx PRIVATE Qt6::Widgets)

install(TARGETS tfx RUNTIME DESTINATION bin)
