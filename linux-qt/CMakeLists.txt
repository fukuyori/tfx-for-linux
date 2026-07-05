cmake_minimum_required(VERSION 3.20)

project(tfx-for-linux VERSION 0.7.2 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)
find_package(qtermwidget6 QUIET)
if(BUILD_TESTING)
    find_package(Qt6 QUIET COMPONENTS Test)
endif()

if(APPLE)
    set(TFX_PLATFORM_SOURCES src/platform/macos/PlatformMacOS.cpp)
elseif(WIN32)
    set(TFX_PLATFORM_SOURCES src/platform/windows/PlatformWindows.cpp)
else()
    set(TFX_PLATFORM_SOURCES src/platform/linux/PlatformLinux.cpp)
endif()

qt_add_executable(tfx
    src/core/DelimitedText.cpp
    src/core/DelimitedText.h
    src/core/FileOperations.cpp
    src/core/FileOperations.h
    src/core/FileOperationWorker.cpp
    src/core/FileOperationWorker.h
    src/core/FileTypeInfo.cpp
    src/core/FileTypeInfo.h
    src/core/PreviewText.cpp
    src/core/PreviewText.h
    src/core/GitService.cpp
    src/core/GitService.h
    src/core/SearchState.cpp
    src/core/SearchState.h
    src/core/TabState.cpp
    src/core/TabState.h
    src/controllers/GitStatusController.cpp
    src/controllers/GitStatusController.h
    src/CommandOutputPane.cpp
    src/CommandOutputPane.h
    src/models/ColumnLayout.cpp
    src/models/ColumnLayout.h
    src/models/FileColumns.h
    src/models/FileSystemProxyModel.cpp
    src/models/FileSystemProxyModel.h
    src/views/FileViews.cpp
    src/views/FileViews.h
    src/platform/Platform.h
    ${TFX_PLATFORM_SOURCES}
    src/AppConfig.cpp
    src/AppConfig.h
    src/FilePaneActions.cpp
    src/FilePaneArchive.cpp
    src/FilePaneClipboard.cpp
    src/FilePaneClipboard.h
    src/FilePaneCommands.cpp
    src/FilePaneConnections.cpp
    src/FilePaneColumns.cpp
    src/FilePaneColumns.h
    src/FilePaneFileOps.cpp
    src/FilePaneInteraction.cpp
    src/FilePaneNavigation.cpp
    src/FilePane.cpp
    src/FilePane.h
    src/FilePaneSearch.cpp
    src/FilePaneSearchRoles.h
    src/FilePaneState.cpp
    src/FilePaneTabs.cpp
    src/FilePaneUi.cpp
    src/MainWindowDock.cpp
    src/MainWindowFileOps.cpp
    src/MainWindowMenus.cpp
    src/MainWindowSettings.cpp
    src/MainWindowSidebar.cpp
    src/MainWindowSidebar.h
    src/MainWindowTheme.cpp
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

qt_add_resources(tfx "tfx_icons"
    PREFIX "/icons"
    BASE packaging/icons
    FILES
        packaging/icons/tfx-32.png
        packaging/icons/tfx-48.png
        packaging/icons/tfx-64.png
        packaging/icons/tfx-128.png
        packaging/icons/tfx-256.png
)

target_include_directories(tfx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tfx PRIVATE Qt6::Widgets Qt6::DBus)

# Optional: use QTermWidget for an interactive terminal pane when available.
if(qtermwidget6_FOUND)
    target_link_libraries(tfx PRIVATE qtermwidget6)
    target_compile_definitions(tfx PRIVATE TFX_HAVE_QTERMWIDGET)
    message(STATUS "tfx: QTermWidget found — interactive terminal enabled")
else()
    message(STATUS "tfx: QTermWidget not found — using the simple command pane")
endif()
target_compile_definitions(tfx PRIVATE TFX_VERSION="${PROJECT_VERSION}")

install(TARGETS tfx RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/tfx.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/tfx.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
foreach(tfx_icon_size 32 48 64 128 256)
    install(FILES packaging/icons/tfx-${tfx_icon_size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${tfx_icon_size}x${tfx_icon_size}/apps
            RENAME tfx.png)
endforeach()
install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/../README.ja.md
        ${CMAKE_CURRENT_SOURCE_DIR}/../CHANGELOG.md
        ${CMAKE_CURRENT_SOURCE_DIR}/../docs/configuration.md
        ${CMAKE_CURRENT_SOURCE_DIR}/../docs/roadmap.md
        ${CMAKE_CURRENT_SOURCE_DIR}/../docs/packaging.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        RENAME README.linux-qt.md)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.ja.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        RENAME README.linux-qt.ja.md)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    @ONLY)
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# Packaging (CPack). scripts/build_package.sh drives DEB/RPM/tar.gz creation.
set(CPACK_PACKAGE_NAME "tfx")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Terminal-inspired file explorer for Linux")
set(CPACK_PACKAGE_DESCRIPTION "tfx for Linux is a C++/Qt port of the tfx file manager, focused on a terminal-like two-pane file management experience for Linux desktops.")
set(CPACK_PACKAGE_VENDOR "fukuyori")
set(CPACK_PACKAGE_CONTACT "fukuyori <fukuyori.n@gmail.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/fukuyori/tfx")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_STRIP_FILES ON)
# Debian: resolve shared-library dependencies from the built binary; the
# archive/preview helper tools are runtime-optional, so they are Recommends.
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "zip, unzip, poppler-utils")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_GROUP "Applications/File")
include(CPack)

if(BUILD_TESTING AND Qt6Test_FOUND)
    qt_add_executable(tfx_file_operation_worker_test
        tests/FileOperationWorkerTest.cpp
        src/core/FileOperationWorker.cpp
        src/core/FileOperationWorker.h
        src/core/FileOperations.cpp
        src/core/FileOperations.h
    )
    target_include_directories(tfx_file_operation_worker_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tfx_file_operation_worker_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME file_operation_worker COMMAND tfx_file_operation_worker_test)

    qt_add_executable(tfx_app_config_test
        tests/AppConfigTest.cpp
        src/AppConfig.cpp
        src/AppConfig.h
    )
    target_include_directories(tfx_app_config_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tfx_app_config_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME app_config COMMAND tfx_app_config_test)

    qt_add_executable(tfx_path_handling_test
        tests/PathHandlingTest.cpp
        src/core/FileOperations.cpp
        src/core/FileOperations.h
    )
    target_include_directories(tfx_path_handling_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tfx_path_handling_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME path_handling COMMAND tfx_path_handling_test)

    qt_add_executable(tfx_git_service_test
        tests/GitServiceTest.cpp
        src/core/GitService.cpp
        src/core/GitService.h
    )
    target_include_directories(tfx_git_service_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tfx_git_service_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME git_service COMMAND tfx_git_service_test)

    qt_add_executable(tfx_search_state_test
        tests/SearchStateTest.cpp
        src/core/SearchState.cpp
        src/core/SearchState.h
    )
    target_include_directories(tfx_search_state_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tfx_search_state_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME search_state COMMAND tfx_search_state_test)

    qt_add_executable(tfx_tab_state_test
        tests/TabStateTest.cpp
        src/core/TabState.cpp
        src/core/TabState.h
        src/core/FileOperations.cpp
        src/core/FileOperations.h
    )
    target_include_directories(tfx_tab_state_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tfx_tab_state_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tab_state COMMAND tfx_tab_state_test)

    qt_add_executable(tfx_column_layout_test
        tests/ColumnLayoutTest.cpp
        src/models/ColumnLayout.cpp
        src/models/ColumnLayout.h
        src/models/FileColumns.h
    )
    target_include_directories(tfx_column_layout_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tfx_column_layout_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME column_layout COMMAND tfx_column_layout_test)

    qt_add_executable(tfx_platform_security_test
        tests/PlatformSecurityTest.cpp
        ${TFX_PLATFORM_SOURCES}
        src/platform/Platform.h
    )
    target_include_directories(tfx_platform_security_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tfx_platform_security_test PRIVATE Qt6::Core Qt6::DBus Qt6::Gui Qt6::Test)
    add_test(NAME platform_security COMMAND tfx_platform_security_test)
    set_tests_properties(platform_security PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(tfx_preview_security_test
        tests/PreviewSecurityTest.cpp
        src/PreviewPane.cpp
        src/PreviewPane.h
        src/UiText.cpp
        src/UiText.h
        src/core/DelimitedText.cpp
        src/core/DelimitedText.h
        src/core/PreviewText.cpp
        src/core/PreviewText.h
        ${TFX_PLATFORM_SOURCES}
        src/platform/Platform.h
    )
    target_include_directories(tfx_preview_security_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tfx_preview_security_test PRIVATE Qt6::Core Qt6::DBus Qt6::Gui Qt6::Widgets Qt6::Test)
    add_test(NAME preview_security COMMAND tfx_preview_security_test)
    set_tests_properties(preview_security PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(tfx_preview_text_test
        tests/PreviewTextTest.cpp
        src/core/DelimitedText.cpp
        src/core/DelimitedText.h
        src/core/PreviewText.cpp
        src/core/PreviewText.h
    )
    target_include_directories(tfx_preview_text_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tfx_preview_text_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME preview_text COMMAND tfx_preview_text_test)
elseif(BUILD_TESTING)
    message(STATUS "tfx: Qt Test not found — file operation worker tests disabled")
endif()
