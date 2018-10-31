# Copyright Tom Westerhout (c) 2018
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#
#     * Neither the name of Tom Westerhout nor the names of other
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 3.9)
project(WalkingLanczos)

include(CTest)
include(GoogleTest)
include(CheckCXXCompilerFlag)
include(CheckIPOSupported)
include(CMakeDependentOption)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to RelWithDebInfo, use -DCMAKE_BUILD_TYPE= to override.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_OPT_NATIVE_SUPPORTED)
CHECK_CXX_COMPILER_FLAG("-fvisibility=hidden" COMPILER_OPT_HIDDEN_SUPPORTED)

# find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)
find_package(Backtrace REQUIRED)

add_library(Lanczos INTERFACE)

set(Boost_USE_STATIC_LIBS ON)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_link_libraries(Lanczos INTERFACE ${CMAKE_DL_LIBS})
    target_compile_definitions(Lanczos INTERFACE BOOST_STACKTRACE_USE_ADDR2LINE)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_link_libraries(Lanczos INTERFACE backtrace ${CMAKE_DL_LIBS})
    target_compile_definitions(Lanczos INTERFACE BOOST_STACKTRACE_USE_BACKTRACE)
endif()
find_package(Boost 1.65.0 REQUIRED COMPONENTS program_options)

check_ipo_supported(RESULT COMPILER_LTO_SUPPORTED OUTPUT COMPILER_LTO_ERROR)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(LANCZOS_WARNING_FLAGS -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic
        -Wno-c++98-c++11-c++14-compat -Wno-weak-vtables -Wno-unused
        -Wno-exit-time-destructors -Wno-documentation-unknown-command
        -Wno-float-equal -Wno-global-constructors -Wno-padded
        # Used a lot in Boost.Assert for C++03 compatibility
        # -Wno-zero-as-null-pointer-constant
        )
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(LANCZOS_WARNING_FLAGS -pedantic -W -Wall -Wextra -Wcast-align -Wcast-qual
        -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self
        -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept
        -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow
        -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel
        -Wstrict-overflow=1 -Wswitch-default -Wundef -Wno-unused)
endif()

target_include_directories(Lanczos INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(Lanczos SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/external)
target_include_directories(Lanczos SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/external/span-lite/include)
# Getting rid of warning in Boost
target_include_directories(Lanczos SYSTEM INTERFACE ${Boost_INCLUDE_DIRS})
target_link_libraries(Lanczos INTERFACE # Boost::boost
                                        Boost::program_options
                                        Threads::Threads
                                        stdc++fs)
target_compile_definitions(Lanczos INTERFACE BOOST_ENABLE_ASSERT_HANDLER)
target_compile_options(Lanczos INTERFACE ${LANCZOS_WARNING_FLAGS})

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
