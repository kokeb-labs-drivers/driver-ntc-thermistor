#===============================================================================
# NTC_THERMISTOR driver: build settings
#===============================================================================
# Read by the root CMakeLists.txt in both of its modes (plain CMake and the
# ESP-IDF component): target name, version, include directories, sources and
# ESP-IDF requirements. This file is the single source
# of the version; tests/CMakeLists.txt checks every other copy against it.
#===============================================================================

include_guard(GLOBAL)

set(NTC_THERMISTOR_TARGET_NAME "ntc_thermistor")

#===============================================================================
# Version (single source of truth)
#===============================================================================
set(NTC_THERMISTOR_VERSION_MAJOR 0)
set(NTC_THERMISTOR_VERSION_MINOR 1)
set(NTC_THERMISTOR_VERSION_PATCH 0)
set(NTC_THERMISTOR_VERSION "${NTC_THERMISTOR_VERSION_MAJOR}.${NTC_THERMISTOR_VERSION_MINOR}.${NTC_THERMISTOR_VERSION_PATCH}")

#===============================================================================
# Generated version header (build tree, never the source tree)
#===============================================================================
set(NTC_THERMISTOR_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../inc/ntc_thermistor_version.h.in")
set(NTC_THERMISTOR_VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/ntc_thermistor_generated")
set(NTC_THERMISTOR_VERSION_HEADER "${NTC_THERMISTOR_VERSION_HEADER_DIR}/ntc_thermistor_version.h")

# The directory must exist before ESP-IDF's idf_component_register() checks the include paths.
file(MAKE_DIRECTORY "${NTC_THERMISTOR_VERSION_HEADER_DIR}")
configure_file("${NTC_THERMISTOR_VERSION_TEMPLATE}" "${NTC_THERMISTOR_VERSION_HEADER}" @ONLY)

#===============================================================================
# Public include directories, sources, ESP-IDF requirements
#===============================================================================
set(NTC_THERMISTOR_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${NTC_THERMISTOR_VERSION_HEADER_DIR}"
)

# Header-only: the template implementation is src/ntc_thermistor.ipp, included by inc/ntc_thermistor.hpp.
set(NTC_THERMISTOR_SOURCE_FILES)

set(NTC_THERMISTOR_IDF_REQUIRES)
