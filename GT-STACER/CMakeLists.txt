cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

project(GT-STACER
    VERSION 26.4.0
    DESCRIPTION "GT-STACER - Linux System Optimizer and Monitoring"
    HOMEPAGE_URL "https://github.com/SalehGNUTUX/GT-STACER"
    LANGUAGES CXX
)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler warnings and optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O2)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endif()

# Qt6 configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 COMPONENTS
    Core
    Gui
    Widgets
    Charts
    Svg
    Concurrent
    Network
    LinguistTools
    REQUIRED
)

# Installation paths
include(GNUInstallDirs)

# Version info as compile definitions
add_compile_definitions(
    APP_NAME="GT-STACER"
    APP_VERSION="26.04"
    APP_ORGANIZATION="GNUTUX"
    APP_DOMAIN="gnutux.org"
    APP_REPO="https://github.com/SalehGNUTUX/GT-STACER"
)

# Subprojects
add_subdirectory(gt-stacer-core)
add_subdirectory(gt-stacer)
