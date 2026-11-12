#===============================================================================
# NTC_THERMISTOR driver: root CMake
#===============================================================================
# One file, two build systems:
#
# - ESP-IDF (ESP_PLATFORM): this folder is an ESP-IDF component. Point
#   EXTRA_COMPONENT_DIRS at it, check it out under components/, or add it as a
#   component-manager git dependency. menuconfig reads _config/Kconfig.
# - Any other CMake build (host, STM32, RP2040, nRF, ...): one INTERFACE target,
#   ntc_thermistor, consumed as ntc_thermistor::ntc_thermistor both through add_subdirectory() and
#   through find_package(ntc_thermistor) after install.
#
#   cmake -S . -B build -DNTC_THERMISTOR_BUILD_TESTS=ON
#   cmake --build build && ctest --test-dir build --output-on-failure
#===============================================================================
cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/ntc_thermistor_build_settings.cmake)

#===============================================================================
# ESP-IDF component
#===============================================================================
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${NTC_THERMISTOR_SOURCE_FILES}
        INCLUDE_DIRS ${NTC_THERMISTOR_PUBLIC_INCLUDE_DIRS}
        REQUIRES ${NTC_THERMISTOR_IDF_REQUIRES}
        KCONFIG ${CMAKE_CURRENT_LIST_DIR}/_config/Kconfig
    )
    if(NTC_THERMISTOR_SOURCE_FILES)
        target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    else()
        target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
    endif()
    return()
endif()

project(ntc_thermistor_driver
    VERSION ${NTC_THERMISTOR_VERSION}
    DESCRIPTION "C++20 driver for NTC thermistors read through a resistor divider into an ADC port."
    LANGUAGES CXX)

include(GNUInstallDirs)

# ISO C++20 for every target this project builds (tests, fuzzers, examples), never the GNU dialect
# that CMake picks by default (handbook KB-CPP-001, KB-CPP20-001). CXX_EXTENSIONS does not travel
# through an INTERFACE target, so each folder that builds a target sets it as well.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The installed layout keeps the source tree's inc/ + src/ pair, because ntc_thermistor.hpp includes
# "../src/ntc_thermistor.ipp" relative to itself:
#   <prefix>/include/ntc_thermistor/inc/*.hpp, ntc_thermistor_version.h
#   <prefix>/include/ntc_thermistor/src/ntc_thermistor.ipp
set(NTC_THERMISTOR_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/ntc_thermistor/inc")

#===============================================================================
# Target
#===============================================================================
add_library(${NTC_THERMISTOR_TARGET_NAME} INTERFACE)
target_include_directories(${NTC_THERMISTOR_TARGET_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
        $<BUILD_INTERFACE:${NTC_THERMISTOR_VERSION_HEADER_DIR}>
        $<INSTALL_INTERFACE:${NTC_THERMISTOR_INSTALL_INCLUDEDIR}>
)
target_compile_features(${NTC_THERMISTOR_TARGET_NAME} INTERFACE cxx_std_20)

# ntc_thermistor::ntc_thermistor everywhere: the alias for add_subdirectory(), EXPORT_NAME for the installed package.
add_library(ntc_thermistor::ntc_thermistor ALIAS ${NTC_THERMISTOR_TARGET_NAME})
set_target_properties(${NTC_THERMISTOR_TARGET_NAME} PROPERTIES EXPORT_NAME ntc_thermistor)

option(NTC_THERMISTOR_ENABLE_WARNINGS "Add -Wall -Wextra -Wpedantic to every target that links ntc_thermistor::ntc_thermistor" OFF)
if(NTC_THERMISTOR_ENABLE_WARNINGS)
    target_compile_options(${NTC_THERMISTOR_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
endif()

#===============================================================================
# Host tests, documentation snippets, install check (tests/)
#===============================================================================
option(NTC_THERMISTOR_BUILD_TESTS "Build the host tests in tests/" OFF)
option(NTC_THERMISTOR_BUILD_FUZZERS "Also build the libFuzzer targets in tests/fuzz/ (Clang only)" OFF)
if(NTC_THERMISTOR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#===============================================================================
# Install and package export
#===============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS ${NTC_THERMISTOR_TARGET_NAME} EXPORT ntc_thermistorTargets
    INCLUDES DESTINATION ${NTC_THERMISTOR_INSTALL_INCLUDEDIR})

install(DIRECTORY inc/ DESTINATION ${NTC_THERMISTOR_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(FILES "${NTC_THERMISTOR_VERSION_HEADER}" DESTINATION ${NTC_THERMISTOR_INSTALL_INCLUDEDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ntc_thermistor/src
    FILES_MATCHING PATTERN "*.ipp")

install(EXPORT ntc_thermistorTargets
    FILE ntc_thermistorTargets.cmake
    NAMESPACE ntc_thermistor::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ntc_thermistor)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/ntc_thermistorConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ntc_thermistorConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ntc_thermistor)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ntc_thermistorConfigVersion.cmake"
    VERSION ${NTC_THERMISTOR_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ntc_thermistorConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ntc_thermistorConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ntc_thermistor)
