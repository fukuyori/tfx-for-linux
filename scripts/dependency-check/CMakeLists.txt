cmake_minimum_required(VERSION 3.20)
project(tfx_dependency_check LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)
find_package(qtermwidget6 REQUIRED)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

# Compile and link to detect missing development libraries as well as metadata.
add_executable(tfx_dependency_check main.cpp)
target_link_libraries(tfx_dependency_check PRIVATE Qt6::Widgets Qt6::DBus qtermwidget6)
if(BUILD_TESTING)
    target_link_libraries(tfx_dependency_check PRIVATE Qt6::Test)
endif()
