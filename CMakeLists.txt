# ###############################################################################
# MIT License
#
# Copyright (c) 2026 Nico Trost
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# ###############################################################################

cmake_minimum_required(VERSION 3.22)
project(NitecrawlerSDK VERSION 1.1.0 DESCRIPTION "Nitecrawler SDK")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libudev package
if(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUDEV libudev)
endif()

# SDK Library
add_library(NitecrawlerSDK SHARED
    NitecrawlerDevice.cpp
    NitecrawlerProtocol.cpp
    NitecrawlerLogging.cpp
    NitecrawlerSerialPort.cpp
    NitecrawlerSDK.cpp)

# Public headers
target_include_directories(NitecrawlerSDK PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link libudev
if(UNIX)
    if(LIBUDEV_FOUND)
        target_include_directories(NitecrawlerSDK PRIVATE ${LIBUDEV_INCLUDE_DIRS})
        target_link_libraries(NitecrawlerSDK PRIVATE ${LIBUDEV_LIBRARIES})
    else()
        message(FATAL_ERROR "libudev not found. Install it with: sudo apt-get install libudev-dev")
    endif()
endif()

if(WIN32)
    target_link_libraries(NitecrawlerSDK PRIVATE SetupAPI)
    # Prevent Windows headers from defining min/max macros
    target_compile_definitions(NitecrawlerSDK PRIVATE NOMINMAX)
endif()

# Test executables
add_executable(test_nitecrawler test_nitecrawler.cpp)
target_link_libraries(test_nitecrawler NitecrawlerSDK)

# Installation
install(TARGETS NitecrawlerSDK
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES NitecrawlerSDK.h DESTINATION include)

# CPack configuration for packaging
if(UNIX)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_CONTACT "Nico")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/nitr57/NitecrawlerSDK")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Nico")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION_SUMMARY "Nitecrawler SDK - Focuser and Rotator Control Library")

    # Runtime dependencies
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libudev1")

    # License
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

    include(CPack)
endif()
