cmake_minimum_required(VERSION 3.14)

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

# Option to use session bus instead of system bus
option(USE_SESSION_BUS "Use session bus instead of system bus" OFF)

# Create the CAN listener executable
add_executable(canlistenner
    main.cpp
    CANListener.cpp
    CANListener.h
)

# Link with CAN connector library
target_link_libraries(canlistenner PRIVATE can_connector)

# Link with sdbus-c++
target_include_directories(canlistenner PRIVATE ${SDBUSCPP_INCLUDE_DIRS})
target_link_libraries(canlistenner PRIVATE ${SDBUSCPP_LIBRARIES})

# Add compile definitions
target_compile_definitions(canlistenner PRIVATE
    $<$<BOOL:${USE_SESSION_BUS}>:USE_SESSION_BUS=1>)

# Set C++ standard
set_target_properties(canlistenner PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
