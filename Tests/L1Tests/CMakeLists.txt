# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.8)

project(SceneSetL1Tests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(GTest REQUIRED)

# Set the executable name
set(TEST_EXECUTABLE SceneSetL1TestsIN)

# Source files - include the actual SceneSet implementation
set(TEST_SRC
    tests/test_SceneSet.cpp
    ../../src/SceneSet.cpp
   )

# Create the test executable
add_executable(${TEST_EXECUTABLE} ${TEST_SRC})

# WPEFramework library paths
set(WPEFRAMEWORK_LIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)
set(WPEFRAMEWORK_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)

#Link libraries
target_link_libraries(${TEST_EXECUTABLE}
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    ${WPEFRAMEWORK_LIB_DIR}/libWPEFrameworkCore.so
    ${WPEFRAMEWORK_LIB_DIR}/libWPEFrameworkCOM.so
    pthread
    systemd
    dl
)

# Include directories
target_include_directories(${TEST_EXECUTABLE}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${GTEST_INCLUDE_DIRS}
    ${WPEFRAMEWORK_INCLUDE_DIR}
    ${WPEFRAMEWORK_INCLUDE_DIR}/WPEFramework
)

# Add coverage flags for gcc with-coverage builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${TEST_EXECUTABLE}
        PRIVATE
        --coverage
        -fprofile-arcs
        -ftest-coverage
        -g
        -O0
    )
    target_link_options(${TEST_EXECUTABLE}
        PRIVATE
        --coverage
    )
endif()

# Install the binary
install(TARGETS ${TEST_EXECUTABLE} DESTINATION bin)
