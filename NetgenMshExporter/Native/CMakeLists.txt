cmake_minimum_required(VERSION 3.16)

project(NetgenUnityBridge LANGUAGES CXX)

if(DEFINED NETGEN_SOURCE_DIR AND NOT TARGET nglib)
    get_filename_component(NETGEN_SOURCE_DIR "${NETGEN_SOURCE_DIR}" ABSOLUTE)

    if(NOT EXISTS "${NETGEN_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "NETGEN_SOURCE_DIR does not contain a CMakeLists.txt: ${NETGEN_SOURCE_DIR}")
    endif()

    set(USE_SUPERBUILD OFF CACHE BOOL "" FORCE)
    set(USE_GUI OFF CACHE BOOL "" FORCE)
    set(USE_PYTHON OFF CACHE BOOL "" FORCE)
    set(USE_OCC OFF CACHE BOOL "" FORCE)
    set(USE_STLGEOM ON CACHE BOOL "" FORCE)
    set(USE_CSG ON CACHE BOOL "" FORCE)
    set(USE_INTERFACE ON CACHE BOOL "" FORCE)
    set(USE_GEOM2D ON CACHE BOOL "" FORCE)
    set(USE_MPI OFF CACHE BOOL "" FORCE)
    set(USE_NATIVE_ARCH OFF CACHE BOOL "" FORCE)
    set(ENABLE_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_STUB_FILES OFF CACHE BOOL "" FORCE)
    set(NGLIB_LIBRARY_TYPE SHARED CACHE STRING "" FORCE)
    set(NGCORE_LIBRARY_TYPE SHARED CACHE STRING "" FORCE)

    add_subdirectory("${NETGEN_SOURCE_DIR}" netgen-build)
endif()

if(NOT TARGET nglib)
    message(FATAL_ERROR "The nglib target is required. Set NETGEN_SOURCE_DIR or add this project from a Netgen build.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(NetgenUnityBridge SHARED
    NetgenUnityBridge.cpp
    NetgenUnityBridge.h)

target_compile_definitions(NetgenUnityBridge PRIVATE NETGEN_UNITY_BRIDGE_EXPORTS)

target_link_libraries(NetgenUnityBridge PRIVATE nglib)

if(APPLE)
    set_target_properties(NetgenUnityBridge PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_NAME_DIR "@rpath"
        INSTALL_RPATH "@loader_path")
endif()

target_include_directories(NetgenUnityBridge
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${NETGEN_SOURCE_DIR}/nglib
        ${NETGEN_SOURCE_DIR}/libsrc
        ${NETGEN_SOURCE_DIR}/libsrc/include
        ${NETGEN_SOURCE_DIR}/libsrc/core
        ${NETGEN_SOURCE_DIR}/libsrc/stlgeom
        ${CMAKE_CURRENT_BINARY_DIR}/netgen-build
        ${NETGEN_BINARY_DIR})

if(MSVC)
    target_compile_options(NetgenUnityBridge PRIVATE /W4 /permissive-)
endif()
