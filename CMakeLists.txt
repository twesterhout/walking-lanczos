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
include(CMakeDependentOption)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/Vc/cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to RelWithDebInfo, use \
-DCMAKE_BUILD_TYPE= to override."
    )
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(NQS_USE_INTEL_MKL
    "If set to ON, Intel Math Kernel Library will \
be used for BLAS and LAPACK routines."
    ON
)

option(NQS_VERBOSE_BUILD
    "Enables some information messages during the build process."
    ON
)

option(NQS_BUILD_TESTING
    "Enable building of tests."
    ON
)

set(INTEL_ROOT
    ${CMAKE_CURRENT_SOURCE_DIR}/external/intel
    CACHE PATH
    "Root directory of Intel products. This will likely be /opt/intel on Linux."
)

set(MKL_ROOT
    ${INTEL_ROOT}/mkl
    CACHE PATH
    "Root directory of Intel MKL 2018 installation."
)

CMAKE_DEPENDENT_OPTION(MKL_SDL
    "Use Single Dynamically Linked version of Intel MKL."
    OFF
    "NQS_USE_INTEL_MKL" OFF)
CMAKE_DEPENDENT_OPTION(MKL_MULTI_THREADED
    "Use multi-threaded interface of Intel MKL."
    OFF
    "NQS_USE_INTEL_MKL" OFF)
CMAKE_DEPENDENT_OPTION(MKL_GNU_THREAD
    "Use GNU OpenMP rather than Intel OpenMP."
    ON
    "NQS_USE_INTEL_MKL; MKL_MULTI_THREADED" OFF)

find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)
find_package(Backtrace REQUIRED)
# find_package(Vc REQUIRED)
# find_package(MKL REQUIRED)
# find_package(SVML REQUIRED)

# message(STATUS "[Backtrace] Backtrace_LIBRARIES: ${Backtrace_LIBRARY}")
# message(STATUS "[Backtrace] Backtrace_HEADER: ${Backtrace_HEADER}")
# message(STATUS "[Backtrace] Backtrace_INCLUDE_DIRS: ${Backtrace_INCLUDE_DIR}")

# Boost
################################################################################

find_package(Boost 1.68.0 REQUIRED COMPONENTS program_options
    stacktrace_basic stacktrace_addr2line)

# FindBoost.cmake that ships with CMake-3.12 still doesn't support
# Boost::stacktrace, so we have to handle it manually
add_library(boost_stacktrace INTERFACE)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_link_libraries(boost_stacktrace INTERFACE
        Boost::boost
        -L${Boost_LIBRARY_DIR_RELEASE}/libboost_stacktrace_basic.so
        ${CMAKE_DL_LIBS})
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_definitions(boost_stacktrace INTERFACE BOOST_STACKTRACE_USE_BACKTRACE)
    target_link_libraries(boost_stacktrace INTERFACE
        Boost::boost
        -L${Boost_LIBRARY_DIR_RELEASE}/libboost_stacktrace_backtrace.so
        backtrace
        ${CMAKE_DL_LIBS})
endif()

# Intel Thread Building Blocks
################################################################################

find_package(TBB REQUIRED)
add_library(intel_tbb INTERFACE)
target_include_directories(intel_tbb INTERFACE ${TBB_INCLUDE_DIRS})
target_link_libraries(intel_tbb INTERFACE ${TBB_LIBRARIES})
# TBB_INCLUDE_DIRS, where to find task_scheduler_init.h, etc.
# TBB_LIBRARY_DIRS, where to find libtbb, libtbbmalloc
# TBB_DEBUG_LIBRARY_DIRS, where to find libtbb_debug, libtbbmalloc_debug
# TBB_INSTALL_DIR, the base TBB install directory
# TBB_LIBRARIES, the libraries to link against to use TBB.
# TBB_DEBUG_LIBRARIES, the libraries to link against to use TBB with debug symbols.
# TBB_FOUND, If false, don't try to use TBB.
# TBB_INTERFACE_VERSION, as defined in tbb/tbb_stddef.h


# Guidelines Support Library
################################################################################
# set(GSL_TEST OFF)
# add_subdirectory(external/GSL)

CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_OPT_NATIVE_SUPPORTED)
CHECK_CXX_COMPILER_FLAG("-fvisibility=hidden" COMPILER_OPT_HIDDEN_SUPPORTED)

# Warnings
################################################################################
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(NQS_WARNING_FLAGS -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic
        -Wno-c++98-c++11-c++14-compat -Wno-weak-vtables -Wno-unused
        -Wno-exit-time-destructors -Wno-documentation-unknown-command
        -Wno-float-equal -Wno-global-constructors -Wno-padded
        )
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(NQS_WARNING_FLAGS -pedantic -W -Wall -Wextra -Wcast-align -Wcast-qual
        -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self
        -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept
        -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow
        -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel
        -Wstrict-overflow=1 -Wswitch-default -Wundef -Wno-unused)
endif()

add_executable(first first.cpp)
target_link_libraries(first PRIVATE
    Boost::boost Boost::program_options Boost::stacktrace_basic
    Boost::stacktrace_addr2line intel_tbb)

add_library(Lanczos INTERFACE)
target_include_directories(Lanczos INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(Lanczos INTERFACE Boost::boost Boost::program_options
    Boost::stacktrace_addr2line intel_tbb stdc++fs)
target_compile_definitions(Lanczos INTERFACE BOOST_ALL_DYN_LINK
    BOOST_ENABLE_ASSERT_HANDLER BOOST_STACKTRACE_USE_BACKTRACE)
# target_link_libraries(Lanczos INTERFACE -fsanitize=address)
# target_compile_options(Lanczos INTERFACE -fsanitize=address)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
