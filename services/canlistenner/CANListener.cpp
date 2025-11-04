#include "CANListener.h"
#include <iostream>
#include <chrono>
#include <thread>

CANListener* CANListener::instance()
{
    static std::unique_ptr<CANListener> instance;
    if (!instance) {
        instance.reset(new CANListener());
    }
    return instance.get();
}

CANListener::CANListener()
    : m_canConnector(std::make_unique<CANConnector>("vcan0"))
{
    // Set CAN callbacks
    m_canConnector->setMessageCallback([this](uint32_t canId, const std::vector<uint8_t>& data) {
        onCANMessageReceived(canId, data);
    });
    
    m_canConnector->setStatusCallback([this](bool connected) {
        std::cout << "CAN interface " << (connected ? "connected" : "disconnected") << std::endl;
    });
    
    m_canConnector->setErrorCallback([this](const std::string& error) {
        std::cerr << "CAN error: " << error << std::endl;
    });
}

CANListener::~CANListener()
{
    stop();
}

void CANListener::start()
{
    std::cout << "Starting CAN Listener service..." << std::endl;
    
    // Setup D-Bus interface
    setupDBusInterface();
    
    // Connect to CAN interface
    if (!m_canConnector->connect()) {
        std::cerr << "Failed to connect to CAN interface" << std::endl;
        return;
    }
    
    std::cout << "CAN Listener service started successfully" << std::endl;
}

void CANListener::stop()
{
    std::cout << "Stopping CAN Listener service..." << std::endl;
    
    if (m_canConnector) {
        m_canConnector->disconnect();
    }
    
    // Safely stop the D-Bus event loop, join its thread, release the name and
    // reset D-Bus objects. Operations may fail if the connection is already
    // closed; catch and log but do not throw.
    if (m_dbusConnection) {
        try {
            m_dbusConnection->leaveEventLoop();
        } catch (const sdbus::Error& e) {
            std::cerr << "Warning: Failed to leave D-Bus event loop: " << e.getMessage() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Warning: Exception leaving D-Bus event loop: " << e.what() << std::endl;
        }

        if (m_dbusThread && m_dbusThread->joinable()) {
            m_dbusThread->join();
        }

        try {
            m_dbusConnection->releaseName(SERVICE_NAME);
        } catch (const sdbus::Error& e) {
            std::cerr << "Warning: Failed to release D-Bus name: " << e.getMessage() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Warning: Exception while releasing D-Bus name: " << e.what() << std::endl;
        }

        // Reset object and connection to ensure idempotent stop()
        m_dbusObject.reset();
        m_dbusConnection.reset();
        m_dbusThread.reset();
    }

    std::cout << "CAN Listener service stopped" << std::endl;
}

void CANListener::setupDBusInterface()
{
    try {
        // Create D-Bus connection
        m_dbusConnection = sdbus::createSessionBusConnection();
        std::cout << "[CAN Listener] Connected to SESSION bus" << std::endl;

        // Request service name
        m_dbusConnection->requestName(SERVICE_NAME);

        // Create D-Bus object
        m_dbusObject = sdbus::createObject(*m_dbusConnection, OBJECT_PATH);

        // Register methods
        m_dbusObject->registerMethod("SendCANMessage")
            .onInterface(INTERFACE_NAME)
            .withInputParamNames("canId", "data")
            .withOutputParamNames("success")
            .implementedAs([this](uint32_t canId, const std::vector<uint8_t>& data) -> bool {
                return m_canConnector->sendMessage(canId, data);
            });

        m_dbusObject->registerMethod("GetStatus")
            .onInterface(INTERFACE_NAME)
            .withOutputParamNames("status")
            .implementedAs([this]() -> std::string {
                return m_canConnector->isConnected() ? "Connected" : "Disconnected";
            });

        // Register signals
        m_dbusObject->registerSignal("CANMessageReceived")
            .onInterface(INTERFACE_NAME)
            .withParameters<uint32_t, std::vector<uint8_t>, uint64_t>();

        m_dbusObject->registerSignal("CANMessageSent")
            .onInterface(INTERFACE_NAME)
            .withParameters<uint32_t, std::vector<uint8_t>, uint64_t>();

        m_dbusObject->finishRegistration();

        std::cout << "[CAN Listener] D-Bus service ready: " << SERVICE_NAME << std::endl;

        // Start the sdbus event loop in a background thread so this service
        // processes incoming method calls and replies. enterEventLoop blocks,
        // so run it in its own thread.
        m_dbusThread = std::make_unique<std::thread>([this]() {
            try {
                m_dbusConnection->enterEventLoop();
            } catch (const sdbus::Error& e) {
                std::cerr << "D-Bus event loop error: " << e.getMessage() << std::endl;
            }
        });

    } catch (const sdbus::Error& e) {
        std::cerr << "D-Bus setup error: " << e.getMessage() << std::endl;
    }
}

void CANListener::onCANMessageReceived(uint32_t canId, const std::vector<uint8_t>& data)
{
    // Get current timestamp
    auto timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    // Emit D-Bus signal
    try {
        if (m_dbusObject) {
            auto signal = m_dbusObject->createSignal(INTERFACE_NAME, "CANMessageReceived");
            signal << canId << data << timestamp;
            m_dbusObject->emitSignal(signal);
            std::cout << "Emitted D-Bus signal CANMessageReceived with canId=0x" 
                     << std::hex << canId << std::dec << std::endl;
        }
    } catch (const sdbus::Error& e) {
        std::cerr << "Error emitting CAN message signal: " << e.getMessage() << std::endl;
    }

    // Forward to ECU if needed
    forwardCANMessageToECU(canId, data);
    
    std::cout << "CAN message received - ID: 0x" << std::hex << canId << std::dec 
              << " Data: ";
    for (uint8_t byte : data) {
        printf("%02X ", byte);
    }
    std::cout << std::endl;
}

void CANListener::forwardCANMessageToECU(uint32_t canId, const std::vector<uint8_t>& data)
{
    // This method handles forwarding CAN messages to other ECUs
    // Implementation depends on specific ECU communication requirements
    
    // Example: Forward specific CAN IDs to other ECUs
    if (canId >= 0x100 && canId <= 0x1FF) {
        // Forward engine-related messages
        std::cout << "Forwarding engine message to ECU - ID: 0x" << std::hex << canId << std::dec << std::endl;
    } else if (canId >= 0x200 && canId <= 0x2FF) {
        // Forward transmission-related messages
        std::cout << "Forwarding transmission message to ECU - ID: 0x" << std::hex << canId << std::dec << std::endl;
    }
    
    // Add more forwarding logic as needed
}

void CANListener::processAppServerMessage(const std::string& message)
{
    // Process messages from App Server
    // This method can be extended to handle different types of server commands
    
    std::cout << "Processing App Server message: " << message << std::endl;
    
    // Example: Parse JSON or other message format
    // Send appropriate CAN messages based on server commands
}
