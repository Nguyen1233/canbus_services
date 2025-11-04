cmake_minimum_required(VERSION 3.14)

# Find required packages
find_package(PkgConfig REQUIRED)

# Try to find GTest
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    # Try to find GTest using pkg-config
    pkg_check_modules(GTEST gtest)
    if(NOT GTEST_FOUND)
        # Try to find GTest manually
        find_path(GTEST_INCLUDE_DIR gtest/gtest.h
            PATHS /usr/include /usr/local/include
        )
        find_library(GTEST_LIBRARY gtest
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        )
        find_library(GTEST_MAIN_LIBRARY gtest_main
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        )
        
        if(GTEST_INCLUDE_DIR AND GTEST_LIBRARY AND GTEST_MAIN_LIBRARY)
            set(GTest_FOUND TRUE)
            set(GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIR})
            set(GTEST_LIBRARIES ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY})
            message(STATUS "Found GTest manually: ${GTEST_LIBRARIES}")
        else()
            message(WARNING "GTest not found. Please install libgtest-dev:")
            message(WARNING "  sudo apt-get install libgtest-dev")
            message(WARNING "  cd /usr/src/gtest && sudo cmake . && sudo make && sudo make install")
            message(WARNING "Tests will be disabled.")
            return()
        endif()
    else()
        set(GTest_FOUND TRUE)
        message(STATUS "Found GTest via pkg-config: ${GTEST_LIBRARIES}")
    endif()
endif()

# Try to find GMock (optional)
find_package(GMock QUIET)
if(NOT GMock_FOUND)
    pkg_check_modules(GMOCK gmock)
    if(NOT GMOCK_FOUND)
        find_path(GMOCK_INCLUDE_DIR gmock/gmock.h
            PATHS /usr/include /usr/local/include
        )
        find_library(GMOCK_LIBRARY gmock
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        )
        if(GMOCK_INCLUDE_DIR AND GMOCK_LIBRARY)
            set(GMock_FOUND TRUE)
            set(GMOCK_INCLUDE_DIRS ${GMOCK_INCLUDE_DIR})
            set(GMOCK_LIBRARIES ${GMOCK_LIBRARY})
            message(STATUS "Found GMock manually: ${GMOCK_LIBRARIES}")
        else()
            message(STATUS "GMock not found - tests will use GTest only")
            set(GMock_FOUND FALSE)
        endif()
    else()
        set(GMock_FOUND TRUE)
        message(STATUS "Found GMock via pkg-config: ${GMOCK_LIBRARIES}")
    endif()
endif()

# Find sdbus-c++
pkg_check_modules(SDBUSCPP sdbus-c++)
if (NOT SDBUSCPP_FOUND)
  pkg_check_modules(SDBUSCPP sdbus-c++-1)
endif()
if (NOT SDBUSCPP_FOUND)
  message(FATAL_ERROR "sdbus-c++ not found. Install libsdbus-c++-dev.")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/lib)
include_directories(${CMAKE_SOURCE_DIR}/services)

# Link directories
link_directories(${CMAKE_BINARY_DIR}/lib/can)

# Test executables
add_executable(test_can_connector
    test_can_connector.cpp
)

add_executable(test_can_listener
    test_can_listener.cpp
)

# add_executable(test_app_server_bridge
#     test_app_server_bridge.cpp
# )

add_executable(test_integration
    test_integration.cpp
)

# Add service sources to tests to resolve symbols
target_sources(test_can_listener PRIVATE
    ${CMAKE_SOURCE_DIR}/services/canlistenner/CANListener.cpp
)

# target_sources(test_app_server_bridge PRIVATE
#     ${CMAKE_SOURCE_DIR}/services/appserverbridge/AppServerBridge.cpp
# )

target_sources(test_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/services/canlistenner/CANListener.cpp
    # ${CMAKE_SOURCE_DIR}/services/appserverbridge/AppServerBridge.cpp
)

# Set GTest libraries based on what was found
if(GTest_FOUND)
    if(TARGET GTest::GTest)
        set(GTEST_LINK_LIBS GTest::GTest GTest::Main)
    else()
        set(GTEST_LINK_LIBS ${GTEST_LIBRARIES})
    endif()
else()
    set(GTEST_LINK_LIBS ${GTEST_LIBRARIES})
endif()

# Link libraries for CAN connector tests
target_link_libraries(test_can_connector
    can_connector
    ${GTEST_LINK_LIBS}
    pthread
)

# Link libraries for CAN listener tests
target_link_libraries(test_can_listener
    can_connector
    ${SDBUSCPP_LIBRARIES}
    ${GTEST_LINK_LIBS}
    pthread
)

# Link libraries for App Server bridge tests
# target_link_libraries(test_app_server_bridge
#     ${SDBUSCPP_LIBRARIES}
#     ${GTEST_LINK_LIBS}
#     pthread
# )

# Link libraries for integration tests
# target_link_libraries(test_integration
#     can_connector
#     ${SDBUSCPP_LIBRARIES}
#     ${GTEST_LINK_LIBS}
#     pthread
# )

# Include directories for all test targets
if(GTest_FOUND)
    if(TARGET GTest::GTest)
        target_link_libraries(test_can_connector GTest::GTest GTest::Main)
        target_link_libraries(test_can_listener GTest::GTest GTest::Main)
        # target_link_libraries(test_app_server_bridge GTest::GTest GTest::Main)
        target_link_libraries(test_integration GTest::GTest GTest::Main)
    else()
        target_include_directories(test_can_connector PRIVATE ${GTEST_INCLUDE_DIRS})
        target_include_directories(test_can_listener PRIVATE ${GTEST_INCLUDE_DIRS})
        target_include_directories(test_app_server_bridge PRIVATE ${GTEST_INCLUDE_DIRS})
        target_include_directories(test_integration PRIVATE ${GTEST_INCLUDE_DIRS})
    endif()
endif()

target_include_directories(test_can_connector PRIVATE ${SDBUSCPP_INCLUDE_DIRS})
target_include_directories(test_can_listener PRIVATE ${SDBUSCPP_INCLUDE_DIRS})
# target_include_directories(test_app_server_bridge PRIVATE ${SDBUSCPP_INCLUDE_DIRS})
target_include_directories(test_integration PRIVATE ${SDBUSCPP_INCLUDE_DIRS})

# Compiler flags
target_compile_options(test_can_connector PRIVATE ${SDBUSCPP_CFLAGS_OTHER})
target_compile_options(test_can_listener PRIVATE ${SDBUSCPP_CFLAGS_OTHER})
# target_compile_options(test_app_server_bridge PRIVATE ${SDBUSCPP_CFLAGS_OTHER})
target_compile_options(test_integration PRIVATE ${SDBUSCPP_CFLAGS_OTHER})

# Add library dependencies
target_link_libraries(test_integration 
    can_connector
    ${SDBUSCPP_LIBRARIES}
    pthread
)

# Enable testing
enable_testing()

# Add tests
add_test(NAME CANConnectorTests COMMAND test_can_connector)
add_test(NAME CANListenerTests COMMAND test_can_listener)
add_test(NAME AppServerBridgeTests COMMAND test_app_server_bridge)
add_test(NAME IntegrationTests COMMAND test_integration)

# Set test properties
set_tests_properties(CANConnectorTests PROPERTIES TIMEOUT 30)
set_tests_properties(CANListenerTests PROPERTIES TIMEOUT 30)
set_tests_properties(AppServerBridgeTests PROPERTIES TIMEOUT 30)
set_tests_properties(IntegrationTests PROPERTIES TIMEOUT 60)

# Print test configuration
message(STATUS "Test Configuration:")
message(STATUS "  GTest found: ${GTest_FOUND}")
message(STATUS "  sdbus-c++ found: ${SDBUSCPP_FOUND}")
message(STATUS "  Test executables: test_can_connector, test_can_listener, test_app_server_bridge, test_integration")
