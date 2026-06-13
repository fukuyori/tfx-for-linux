cmake_minimum_required(VERSION 3.20)

project(tfx-for-linux VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)

if(APPLE)
    set(TFX_PLATFORM_SOURCES src/platform/macos/PlatformMacOS.cpp)
elseif(WIN32)
    set(TFX_PLATFORM_SOURCES src/platform/windows/PlatformWindows.cpp)
else()
    set(TFX_PLATFORM_SOURCES src/platform/linux/PlatformLinux.cpp)
endif()

qt_add_executable(tfx
    src/core/FileOperations.cpp
    src/core/FileOperations.h
    src/core/FileTypeInfo.cpp
    src/core/FileTypeInfo.h
    src/core/GitService.cpp
    src/core/GitService.h
    src/platform/Platform.h
    ${TFX_PLATFORM_SOURCES}
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

target_include_directories(tfx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tfx PRIVATE Qt6::Widgets Qt6::DBus)
target_compile_definitions(tfx PRIVATE TFX_VERSION="${PROJECT_VERSION}")

install(TARGETS tfx RUNTIME DESTINATION bin)
