cmake_minimum_required(VERSION 3.20)
project(parsecpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Header-only library target
add_library(parsecpp INTERFACE)
target_include_directories(parsecpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(parsecpp INTERFACE cxx_std_23)

# Options
option(PARSECPP_BUILD_TESTS "Build tests" ON)
option(PARSECPP_BUILD_EXAMPLES "Build examples" ON)

if(PARSECPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PARSECPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install
include(GNUInstallDirs)
install(TARGETS parsecpp EXPORT parsecpp-targets)
install(DIRECTORY include/parsecpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT parsecpp-targets
    FILE parsecppConfig.cmake
    NAMESPACE parsecpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/parsecpp
)
