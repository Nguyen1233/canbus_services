# DMS Service Tests

This directory contains comprehensive unit and integration tests for the DMS Service system, specifically testing the flow from vcan0 to D-Bus session bus.

## Test Structure

### Unit Tests

1. **test_can_connector.cpp** - Tests for CANConnector class
   - Basic connection/disconnection
   - Message sending and receiving
   - Error handling
   - Thread safety
   - Interface management

2. **test_can_listener.cpp** - Tests for CANListener service
   - Singleton pattern
   - D-Bus interface setup
   - Service lifecycle
   - Message processing

3. **test_app_server_bridge.cpp** - Tests for AppServerBridge service
   - Singleton pattern
   - D-Bus interface setup
   - Server connection
   - Message forwarding
   - Error handling

### Integration Tests

4. **test_integration.cpp** - End-to-end integration tests
   - Complete vcan0 to D-Bus flow
   - Bidirectional communication
   - Multiple message handling
   - Service restart scenarios
   - High-frequency message flow

## Prerequisites

### System Requirements
- Linux system with CAN support
- D-Bus session bus running
- Root privileges for vcan0 interface setup
- Google Test framework (libgtest-dev)
- sdbus-c++ library (libsdbus-c++-dev)

### Installation
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y libgtest-dev libsdbus-c++-dev cmake build-essential

# Install Google Test
cd /usr/src/gtest
sudo cmake .
sudo make
sudo make install
```

## Running Tests

### Quick Start
```bash
# Make script executable
chmod +x run_tests.sh

# Run all tests (requires sudo for vcan0 setup)
sudo ./run_tests.sh
```

### Manual Setup
```bash
# Setup vcan0 interface
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Build tests
mkdir -p build
cd build
cmake ..
make -j$(nproc)

# Run individual test suites
./test_can_connector
./test_can_listener
./test_app_server_bridge
./test_integration

# Cleanup
sudo ip link set down vcan0
sudo ip link delete vcan0
```

### Using CTest
```bash
cd build
ctest --verbose
```

## Test Coverage

### CAN Connector Tests
- ✅ Connection to vcan0 interface
- ✅ Message sending/receiving
- ✅ Error handling for invalid interfaces
- ✅ Oversized message handling
- ✅ Thread safety
- ✅ Reconnection scenarios

### CAN Listener Tests
- ✅ Singleton pattern verification
- ✅ D-Bus service registration
- ✅ Service lifecycle management
- ✅ Concurrent access safety

### App Server Bridge Tests
- ✅ Singleton pattern verification
- ✅ D-Bus service registration
- ✅ TCP server connection
- ✅ Message forwarding to server
- ✅ Server message processing
- ✅ Heartbeat functionality

### Integration Tests
- ✅ Complete vcan0 → CAN Listener → D-Bus → App Server Bridge flow
- ✅ Bidirectional communication
- ✅ Multiple message handling
- ✅ Service restart scenarios
- ✅ High-frequency message flow
- ✅ Error handling in complete flow

## Test Scenarios

### Basic Flow Test
1. Send CAN message on vcan0
2. Verify message is received by CAN Listener
3. Verify D-Bus signal is emitted
4. Verify message is forwarded to App Server
5. Verify server receives the message

### Error Handling Test
1. Send invalid CAN message (oversized)
2. Verify error is handled gracefully
3. Verify system continues to function

### Service Restart Test
1. Start services
2. Send messages
3. Restart services
4. Verify system recovers and continues working

### High Frequency Test
1. Send many messages quickly
2. Verify most messages are processed
3. Verify system stability

## D-Bus Services Tested

### CAN Listener Service
- **Service Name**: `org.example.DMS.CAN`
- **Object Path**: `/org/example/DMS/CANListener`
- **Interface**: `org.example.DMS.CAN`
- **Methods**: `SendCANMessage`, `GetStatus`
- **Signals**: `CANMessageReceived`, `CANMessageSent`

### App Server Bridge Service
- **Service Name**: `org.example.DMS.AppServer`
- **Object Path**: `/org/example/DMS/AppServerBridge`
- **Interface**: `org.example.DMS.AppServer`
- **Methods**: `SendCANMessage`, `GetServerStatus`
- **Signals**: `ServerConnected`, `ServerDisconnected`, `ServerMessageReceived`

## Troubleshooting

### Common Issues

1. **vcan0 interface not found**
   ```bash
   sudo modprobe vcan
   sudo ip link add dev vcan0 type vcan
   sudo ip link set up vcan0
   ```

2. **D-Bus session bus not available**
   - Make sure you're running in a desktop environment
   - Check if D-Bus session bus is running: `dbus-send --session --dest=org.freedesktop.DBus --type=method_call /org/freedesktop/DBus org.freedesktop.DBus.GetId`

3. **Permission denied for CAN socket**
   - Make sure vcan0 interface exists and is up
   - Check if CAN modules are loaded: `lsmod | grep can`

4. **Tests fail with timeout**
   - Increase timeout values in test files
   - Check system performance
   - Verify all dependencies are installed

### Debug Mode
```bash
# Run tests with verbose output
cd build
./test_can_connector --gtest_verbose
./test_integration --gtest_verbose
```

## Contributing

When adding new tests:

1. Follow the existing test structure
2. Use descriptive test names
3. Include proper setup/teardown
4. Test both success and error cases
5. Add integration tests for new features
6. Update this README with new test coverage

## Test Results

Expected test results:
- All unit tests should pass
- Integration tests should pass (may require vcan0 setup)
- No memory leaks or crashes
- Proper cleanup of resources
- D-Bus services properly registered and unregistered
