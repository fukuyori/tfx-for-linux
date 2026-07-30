option(TFX_ENABLE_RUST_CORE "Use the experimental Rust core implementation" OFF)

function(tfx_enable_rust_core target)
    if(NOT TFX_ENABLE_RUST_CORE)
        return()
    endif()

    target_compile_definitions(${target} PRIVATE TFX_ENABLE_RUST_CORE)
    target_include_directories(${target} PRIVATE ${TFX_RUST_CXXBRIDGE_INCLUDE_DIR})
    target_link_libraries(${target} PRIVATE tfx_rust_bridge)
    add_dependencies(${target} tfx_rust_bridge_build)
endfunction()

if(NOT TFX_ENABLE_RUST_CORE)
    return()
endif()

message(STATUS "tfx: experimental Rust core enabled")

find_program(TFX_CARGO_EXECUTABLE cargo REQUIRED)

set(TFX_RUST_WORKSPACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../rust")
set(TFX_RUST_MANIFEST "${TFX_RUST_WORKSPACE_DIR}/Cargo.toml")
set(TFX_RUST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo-target")
set(TFX_RUST_CXXBRIDGE_INCLUDE_DIR "${TFX_RUST_TARGET_DIR}/cxxbridge")

if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
    set(TFX_RUST_CARGO_PROFILE release)
    set(TFX_RUST_CARGO_PROFILE_ARG --release)
else()
    set(TFX_RUST_CARGO_PROFILE debug)
    set(TFX_RUST_CARGO_PROFILE_ARG)
endif()

set(TFX_RUST_BRIDGE_LIBRARY
    "${TFX_RUST_TARGET_DIR}/${TFX_RUST_CARGO_PROFILE}/${CMAKE_STATIC_LIBRARY_PREFIX}tfx_bridge${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(TFX_RUST_BRIDGE_HEADER
    "${TFX_RUST_CXXBRIDGE_INCLUDE_DIR}/tfx-bridge/src/lib.rs.h")

file(GLOB_RECURSE TFX_RUST_SOURCES CONFIGURE_DEPENDS
    "${TFX_RUST_WORKSPACE_DIR}/Cargo.toml"
    "${TFX_RUST_WORKSPACE_DIR}/Cargo.lock"
    "${TFX_RUST_WORKSPACE_DIR}/crates/*.toml"
    "${TFX_RUST_WORKSPACE_DIR}/crates/*.rs")

add_custom_command(
    OUTPUT ${TFX_RUST_BRIDGE_LIBRARY} ${TFX_RUST_BRIDGE_HEADER}
    COMMAND ${CMAKE_COMMAND} -E env
            CARGO_TARGET_DIR=${TFX_RUST_TARGET_DIR}
            ${TFX_CARGO_EXECUTABLE} build
            --manifest-path ${TFX_RUST_MANIFEST}
            --package tfx-bridge
            --locked
            ${TFX_RUST_CARGO_PROFILE_ARG}
    DEPENDS ${TFX_RUST_SOURCES}
    WORKING_DIRECTORY ${TFX_RUST_WORKSPACE_DIR}
    COMMENT "Building the Rust core bridge"
    VERBATIM
    USES_TERMINAL)

add_custom_target(tfx_rust_bridge_build
    DEPENDS ${TFX_RUST_BRIDGE_LIBRARY} ${TFX_RUST_BRIDGE_HEADER})

add_library(tfx_rust_bridge STATIC IMPORTED GLOBAL)
set_target_properties(tfx_rust_bridge PROPERTIES
    IMPORTED_LOCATION ${TFX_RUST_BRIDGE_LIBRARY})
add_dependencies(tfx_rust_bridge tfx_rust_bridge_build)

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(tfx_rust_bridge INTERFACE
        Threads::Threads
        ${CMAKE_DL_LIBS}
        m)
endif()

if(BUILD_TESTING)
    add_test(
        NAME rust_core
        COMMAND ${CMAKE_COMMAND} -E env
                CARGO_TARGET_DIR=${TFX_RUST_TARGET_DIR}
                ${TFX_CARGO_EXECUTABLE} test
                --manifest-path ${TFX_RUST_MANIFEST}
                --workspace
                --locked)
endif()
