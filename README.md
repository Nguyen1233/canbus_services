# CAN Service - CAN Bus Communication System (Pure C++)

DMS Service is a CAN-Bus communication system designed to receive and transmit CAN messages between an App Server and ECUs in the vehicle. **This edition is a pure C++ implementation with no Qt dependencies.**

## Project Structure

```
DMS_Service/
├── lib/
│   └── can/                    # CAN Connector Library (Pure C++)
│       ├── CMakeLists.txt
│       ├── CANConnector.h
│       └── CANConnector.cpp
├── services/
│   ├── canlistenner/          # CAN Bus Listener Service (Pure C++)
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp
│   │   ├── CANListener.h
│   │   └── CANListener.cpp
│   └── triggerdata/      
│       ├── send_can_dbus_example.py
└── CMakeLists.txt
```

## Main Features

### 1. CAN Bus Listener Service (`canlistenner`)
- Connects to a CAN interface (default: `can0`)
- Receives CAN messages from ECUs
- Emits D-Bus signals when a new CAN message arrives
- Sends CAN messages to other ECUs
- Forwards CAN messages between ECUs
- **Implemented using C++ threading and socket programming**


### 3. CAN Connector Library (`can_connector`)
- Low-level CAN socket interface
- Supports Linux SocketCAN
- Thread-safe CAN communication primitives
- Error handling and reconnection logic
- **Pure C++ implementation using std::thread**

## Dependencies

### Required Packages
- **sdbus-c++** (D-Bus communication)
- **Linux SocketCAN** (CAN support)
- **CMake 3.14+**
- **C++17 compiler**

### Installation
```bash
# Ubuntu/Debian
sudo apt-get install libsdbus-c++-dev cmake build-essential

# Build
mkdir build && cd build
cmake ..
make
```

## Usage

### Quick run & verify (copy-paste commands)

Run everything in a private session D-Bus to avoid name collisions (recommended):

```bash
cd /home/pdnguyen/Documents/TMA/example/DMS_Service
dbus-run-session -- bash
```

Then in that session follow these steps (each can be run in its own terminal window/tab):

1) Bring up `vcan0` (requires sudo)

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan || true
sudo ip link set up vcan0
ip -details link show vcan0
```

2) Start the CAN listener service (logs redirected to a file)

```bash
./build/services/canlistenner/canlistenner 2>&1 | tee /tmp/canlistenner.log &
sleep 0.2
tail -f /tmp/canlistenner.log
```

3) Check D-Bus status (example)

```bash
dbus-send --session --print-reply \
    --dest=org.example.DMS.CAN /org/example/DMS/CANListener \
    org.example.DMS.CAN.GetStatus
# -> ('Connected',) or ('Disconnected',)
```

4) Send a CAN message via D-Bus to the CAN listener (this injects on vcan0)

```bash
dbus-send --session --print-reply --dest=org.example.DMS.CAN \
    /org/example/DMS/CANListener \
    org.example.DMS.CAN.SendCANMessage \
    uint32:291 array:byte:01,02,03,04
```

5) Monitor D-Bus signals (CAN messages emitted by the listener)

```bash
gdbus monitor --session --dest org.example.DMS.CAN --object-path /org/example/DMS/CANListener
# Or
dbus-monitor --session "type='method_call',interface='org.example.DMS.CAN'"
# You will see CANMessageReceived signals with the canId and data
```

Notes:
- Use `dbus-run-session` to avoid D-Bus name collisions with other session services.

### 3. D-Bus Interface

#### CAN Listener D-Bus Interface
- Service: `org.example.DMS.CAN`
- Object Path: `/org/example/DMS/CAN`
- Interface: `org.example.DMS.CAN`

**Methods:**
- `SendCANMessage(uint32_t canId, vector<uint8_t> data) -> bool`
- `GetStatus() -> string`

**Signals:**
- `CANMessageReceived(uint32_t canId, vector<uint8_t> data, uint64_t timestamp)`
- `CANMessageSent(uint32_t canId, vector<uint8_t> data, uint64_t timestamp)`


## CAN Message Format

### CAN ID Ranges
- `0x100-0x1FF`: Engine-related messages
- `0x200-0x2FF`: Transmission-related messages
- `0x300-0x3FF`: Body control messages
- `0x400-0x4FF`: Safety system messages


## Configuration

### CAN Interface
- Default: `can0`
- Can be changed in code or via command line arguments


### D-Bus Bus
- Default: System Bus
- Can be switched to Session Bus by defining `USE_SESSION_BUS=1`

## Pure C++ Features

### Threading
 - Uses `std::thread` for CAN reading and network communication
 - `std::mutex` for thread-safe operations
 - `std::atomic` for thread-safe flags


### Testing
```bash
# Test CAN message sending
cansend can0 123#01020304
timeout 20 dbus-run-session -- build/tests/test_app_server_bridge --gtest_filter=AppServerBridgeTest.SingletonInstance &> /tmp/test_app_bridge_debug.log || true; echo '=== EXIT ==='; tail -n +1 /tmp/test_app_bridge_debug.log

# Test D-Bus communication
dbus-send --system --dest=org.example.DMS.CAN --print-reply /org/example/DMS/CAN org.example.DMS.CAN.SendCANMessage uint32:123 array:byte:1,2,3,4

#rm -rf build/* && cmake -E remove_directory build/
