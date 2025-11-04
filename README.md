# DMS Service - CAN Bus Communication System (Pure C++)

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
│   └── appserverbridge/       # App Server Bridge Service (Pure C++)
│       ├── CMakeLists.txt
│       ├── main.cpp
│       ├── AppServerBridge.h
│       └── AppServerBridge.cpp
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

### 2. App Server Bridge Service (`appserverbridge`)
- Connects via TCP to an App Server (default: `localhost:8080`)
- Receives commands from the App Server
- Sends CAN messages received from the App Server to the CAN interface
- Sends status and heartbeat messages back to the App Server
- Parses and produces JSON messages
- **Implemented using C++ networking and threading**

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

1) Start a minimal mock TCP server (the AppServerBridge connects to this)

```bash
# Run in a separate terminal or background the following Python snippet
python3 -u -c "import socket, time
s=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR,1)
s.bind(('127.0.0.1',8080))
s.listen(1)
print('TCP test server listening on 127.0.0.1:8080')
conn=None
try:
    conn,addr=s.accept()
    print('Accepted connection from',addr)
    while True:
        data=conn.recv(4096)
        if not data:
            break
        print('Received from bridge:',data.decode(errors='replace'))
        conn.sendall(data)
except Exception as e:
    print('Server exception',e)
finally:
    if conn: conn.close()
    s.close()
" &>/tmp/tcp_test_server.log & echo $! > /tmp/tcp_test_server.pid
```

2) Bring up `vcan0` (requires sudo)

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan || true
sudo ip link set up vcan0
ip -details link show vcan0
```

3) Start the CAN listener service (logs redirected to a file)

```bash
./build/services/canlistenner/canlistenner 2>&1 | tee /tmp/canlistenner.log &
sleep 0.2
tail -f /tmp/canlistenner.log
```

4) Start the App Server bridge (logs redirected to a file)

```bash
./build/services/appserverbridge/appserverbridge 2>&1 | tee /tmp/appserverbridge.log &
sleep 0.5
tail -f /tmp/appserverbridge.log
```

5) Check D-Bus status (example)

```bash
gdbus call --session --dest org.example.DMS.AppServer \
    --object-path /org/example/DMS/AppServerBridge \
    --method org.example.DMS.AppServer.GetServerStatus
# -> ('Connected',) or ('Disconnected',)
```

6) Send a CAN message via D-Bus to the CAN listener (this injects on vcan0)

```bash
gdbus call --session --dest org.example.DMS.CAN \
    --object-path /org/example/DMS/CANListener \
    --method org.example.DMS.CAN.SendCANMessage \
    uint32 291 @ay [0x01,0x02,0x03,0x04]
```

Or using `dbus-send`:

```bash
dbus-send --session --print-reply --dest=org.example.DMS.CAN \
    /org/example/DMS/CANListener org.example.DMS.CAN.SendCANMessage \
    uint32:291 array:byte:01,02,03,04
```

7) Send a CAN message to the AppServerBridge (it will forward to the TCP server)

```bash
gdbus call --session --dest org.example.DMS.AppServer \
    --object-path /org/example/DMS/AppServerBridge \
    --method org.example.DMS.AppServer.SendCANMessage \
    uint32 291 @ay [0x01,0x02,0x03,0x04]
```

8) Monitor D-Bus signals (CAN messages emitted by the listener)

```bash
gdbus monitor --session --dest org.example.DMS.CAN --object-path /org/example/DMS/CANListener
# You will see CANMessageReceived signals with the canId and data
```

9) Inspect logs / mock server output

- `tail -f /tmp/canlistenner.log`
- `tail -f /tmp/appserverbridge.log`
- check the terminal running the Python mock TCP server for JSON messages forwarded by the bridge

10) Cleanup (stop services and remove vcan0 if desired)

```bash
pkill -f appserverbridge || true
pkill -f canlistenner || true
sudo ip link set down vcan0 || true
sudo ip link delete vcan0 type vcan || true
```

Notes:
- Use `dbus-run-session` to avoid D-Bus name collisions with other session services.
- If port 8081 is already used, either kill the occupant (`ss -ltnp | grep 8081`) or change the mock server/bridge configuration.


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

#### App Server Bridge D-Bus Interface
- Service: `org.example.DMS.AppServer`
- Object Path: `/org/example/DMS/AppServer`
- Interface: `org.example.DMS.AppServer`

**Methods:**
- `SendCANMessage(uint32_t canId, vector<uint8_t> data) -> bool`
- `GetServerStatus() -> string`

**Signals:**
- `ServerConnected()`
- `ServerDisconnected()`
- `ServerMessageReceived(string message)`

## CAN Message Format

### CAN ID Ranges
- `0x100-0x1FF`: Engine-related messages
- `0x200-0x2FF`: Transmission-related messages
- `0x300-0x3FF`: Body control messages
- `0x400-0x4FF`: Safety system messages

### App Server Message Format (JSON)
```json
{
    "type": "can_command",
    "canId": 256,
    "data": "01020304",
    "timestamp": 1234567890
}
```

## Configuration

### CAN Interface
- Default: `can0`
- Can be changed in code or via command line arguments

### App Server Connection
- Host: `localhost`
- Port: `8080`
 - Can be configured in `AppServerBridge.cpp`

### D-Bus Bus
- Default: System Bus
- Can be switched to Session Bus by defining `USE_SESSION_BUS=1`

## Pure C++ Features

### Threading
 - Uses `std::thread` for CAN reading and network communication
 - `std::mutex` for thread-safe operations
 - `std::atomic` for thread-safe flags

### Networking
 - Raw socket programming with `socket()`, `connect()`, `send()`, `recv()`
 - `select()` for non-blocking I/O
 - Manual connection management and reconnection

### Memory Management
 - Smart pointers (`std::unique_ptr`, `std::shared_ptr`)
 - RAII pattern for resource management
 - No Qt memory management

### Callbacks
 - `std::function` for callback mechanisms
 - Lambda functions for event handling
 - No Qt signals/slots

## Troubleshooting

### CAN Interface Issues
1. Check whether the CAN interface is up:
    ```bash
    ip link show can0
    ```

2. Bring up the CAN interface:
    ```bash
    sudo ip link set can0 up type can bitrate 500000
    ```

### D-Bus Issues
1. Kiểm tra D-Bus service:
   ```bash
   dbus-send --system --dest=org.example.DMS.CAN --print-reply /org/example/DMS/CAN org.example.DMS.CAN.GetStatus
   ```

### App Server Connection Issues
1. Check whether the App Server is running on port 8080
2. Check firewall settings
3. Check logs to debug connection issues

## Development

### Adding New CAN Message Types
1. Update CAN ID ranges in `CANListener.cpp`
2. Add message processing logic in `forwardCANMessageToECU()`

### Adding New App Server Commands
1. Update `processServerMessage()` in `AppServerBridge.cpp`
2. Add new JSON message types

### Testing
```bash
# Test CAN message sending
cansend can0 123#01020304
timeout 20 dbus-run-session -- build/tests/test_app_server_bridge --gtest_filter=AppServerBridgeTest.SingletonInstance &> /tmp/test_app_bridge_debug.log || true; echo '=== EXIT ==='; tail -n +1 /tmp/test_app_bridge_debug.log

# Test D-Bus communication
dbus-send --system --dest=org.example.DMS.CAN --print-reply /org/example/DMS/CAN org.example.DMS.CAN.SendCANMessage uint32:123 array:byte:1,2,3,4
```

## Performance Benefits

### Pure C++ Advantages
- **Faster startup time**: No Qt framework initialization
- **Lower memory footprint**: No Qt overhead
- **Better performance**: Direct system calls
- **Easier deployment**: Fewer dependencies
- **Cross-platform**: Standard C++ libraries only

### Resource Usage
- Minimal memory usage (~2-5MB per service)
- Low CPU overhead
- Efficient threading model
- Direct socket programming
