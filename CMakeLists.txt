cmake_minimum_required(VERSION 3.14)
project(DMS_Service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# Find sdbus-c++
pkg_check_modules(SDBUSCPP sdbus-c++)
if (NOT SDBUSCPP_FOUND)
  pkg_check_modules(SDBUSCPP sdbus-c++-1)
endif()
if (NOT SDBUSCPP_FOUND)
  message(FATAL_ERROR "sdbus-c++ not found. Install libsdbus-c++-dev.")
endif()

# Global options
option(USE_SESSION_BUS "Use session bus instead of system bus" OFF)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/lib)

# Build CAN connector library
add_subdirectory(lib/can)

# Build services
add_subdirectory(services/canlistenner)
# add_subdirectory(services/appserverbridge)

# Build tests
add_subdirectory(tests)

# Create a common target for all DMS services
add_custom_target(dms_services ALL
    DEPENDS canlistenner
    COMMENT "Building all DMS services"
)

# Installation rules
# install(TARGETS canlistenner appserverbridge
#     RUNTIME DESTINATION bin
# )

install(TARGETS can_connector
    LIBRARY DESTINATION lib
)

# Print build information
message(STATUS "DMS Service Configuration:")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Use session bus: ${USE_SESSION_BUS}")
message(STATUS "  sdbus-c++ found: ${SDBUSCPP_FOUND}")