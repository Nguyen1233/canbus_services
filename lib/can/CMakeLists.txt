cmake_minimum_required(VERSION 3.14)

# CAN Connector Library
add_library(can_connector SHARED
    CANConnector.cpp
    CANConnector.h
)

target_include_directories(can_connector PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with pthread for threading
find_package(Threads REQUIRED)
target_link_libraries(can_connector PRIVATE Threads::Threads)

# Set C++ standard
set_target_properties(can_connector PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
