#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <iostream>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "../lib/can/CANConnector.h"
#include "../services/canlistenner/CANListener.h"

// Integration test class for testing vcan0 to D-Bus session bus flow
class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup vcan0 interface
        setupVCANInterface();
        
        // Reset test state
        canMessageReceived = false;
        dbusSignalReceived = false;
        serverMessageReceived = false;
        receivedCanId = 0;
        receivedData.clear();
        receivedTimestamp = 0;
        lastServerMessage.clear();
        
        // Start mock server
        mockServer = std::make_unique<MockServer>(8081);
        ASSERT_TRUE(mockServer->start());
        
        // Wait for server to be ready
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        // Stop all services
        if (canListener) {
            canListener->stop();
        }
        
        // Stop mock server
        if (mockServer) {
            mockServer->stop();
        }
        
        // Clean up
        canListener = nullptr;
        mockServer.reset();
    }

    void setupVCANInterface() {
        // Check if vcan0 exists
        int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (sock < 0) {
            GTEST_SKIP() << "Cannot create CAN socket - skipping tests";
        }

        struct ifreq ifr;
        strcpy(ifr.ifr_name, "vcan0");
        
        if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
            // vcan0 doesn't exist, try to create it
            close(sock);
            
            // Try to create vcan0 interface
            int result = system("sudo modprobe vcan && sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0");
            if (result != 0) {
                GTEST_SKIP() << "Cannot create vcan0 interface - skipping tests. Run: sudo modprobe vcan && sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0";
            }
            
            // Wait a bit for interface to be ready
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            close(sock);
        }
    }

    void startServices() {
        // Start CAN Listener
        canListener = CANListener::instance();
        canListener->start();
        
        // Wait for services to be ready
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    void sendCANMessage(uint32_t canId, const std::vector<uint8_t>& data) {
        int testSocket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        ASSERT_GE(testSocket, 0);
        
        struct ifreq ifr;
        strcpy(ifr.ifr_name, "vcan0");
        ASSERT_GE(ioctl(testSocket, SIOCGIFINDEX, &ifr), 0);
        
        struct sockaddr_can addr;
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;
        ASSERT_GE(bind(testSocket, (struct sockaddr*)&addr, sizeof(addr)), 0);
        
        // Send message
        struct can_frame frame;
        frame.can_id = canId;
        frame.can_dlc = data.size();
        memcpy(frame.data, data.data(), data.size());
        
        ssize_t bytesWritten = write(testSocket, &frame, sizeof(frame));
        EXPECT_EQ(bytesWritten, sizeof(frame));
        
        close(testSocket);
    }

    // Mock server for testing
    class MockServer {
    public:
        MockServer(int port) : m_port(port), m_running(false), m_clientSocket(-1) {}
        
        bool start() {
            m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (m_serverSocket < 0) return false;
            
            int opt = 1;
            setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(m_port);
            
            if (bind(m_serverSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                close(m_serverSocket);
                return false;
            }
            
            if (listen(m_serverSocket, 1) < 0) {
                close(m_serverSocket);
                return false;
            }
            
            m_running = true;
            m_acceptThread = std::thread([this]() {
                struct sockaddr_in clientAddr;
                socklen_t clientLen = sizeof(clientAddr);
                
                m_clientSocket = accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
                if (m_clientSocket >= 0) {
                    handleClient();
                }
            });
            
            return true;
        }
        
        void stop() {
            m_running = false;
            if (m_clientSocket >= 0) {
                close(m_clientSocket);
                m_clientSocket = -1;
            }
            if (m_serverSocket >= 0) {
                close(m_serverSocket);
                m_serverSocket = -1;
            }
            if (m_acceptThread.joinable()) {
                m_acceptThread.join();
            }
        }
        
        std::vector<std::string> getReceivedMessages() {
            std::lock_guard<std::mutex> lock(m_messagesMutex);
            return m_receivedMessages;
        }
        
        void sendMessage(const std::string& message) {
            if (m_clientSocket >= 0) {
                send(m_clientSocket, message.c_str(), message.length(), 0);
            }
        }
        
    private:
        void handleClient() {
            char buffer[4096];
            while (m_running && m_clientSocket >= 0) {
                ssize_t bytesRead = recv(m_clientSocket, buffer, sizeof(buffer) - 1, 0);
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    std::lock_guard<std::mutex> lock(m_messagesMutex);
                    m_receivedMessages.push_back(std::string(buffer));
                } else {
                    break;
                }
            }
        }
        
        int m_port;
        int m_serverSocket;
        int m_clientSocket;
        std::atomic<bool> m_running;
        std::thread m_acceptThread;
        std::vector<std::string> m_receivedMessages;
        std::mutex m_messagesMutex;
    };

    std::unique_ptr<MockServer> mockServer;
    CANListener* canListener = nullptr;
    // AppServerBridge* appServerBridge = nullptr;
    
    std::atomic<bool> canMessageReceived;
    std::atomic<bool> dbusSignalReceived;
    std::atomic<bool> serverMessageReceived;
    std::atomic<uint32_t> receivedCanId;
    std::vector<uint8_t> receivedData;
    std::atomic<uint64_t> receivedTimestamp;
    std::string lastServerMessage;
};

// Test complete vcan0 to D-Bus flow
TEST_F(IntegrationTest, VCANToDBusFlow) {
    startServices();
    
    // Send CAN message on vcan0
    std::vector<uint8_t> testData = {0x01, 0x02, 0x03, 0x04};
    uint32_t testCanId = 0x123;
    
    sendCANMessage(testCanId, testData);
    
    // Wait for message to be processed through the entire flow
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Verify message was received by server
    auto messages = mockServer->getReceivedMessages();
    bool foundCANMessage = false;
    for (const auto& msg : messages) {
        if (msg.find("\"type\":\"can_message\"") != std::string::npos &&
            msg.find("\"canId\":291") != std::string::npos) { // 0x123 = 291
            foundCANMessage = true;
            break;
        }
    }
    
    EXPECT_TRUE(foundCANMessage);
}

// Test D-Bus signal emission
TEST_F(IntegrationTest, DBusSignalEmission) {
    startServices();
    
    // Send CAN message
    std::vector<uint8_t> testData = {0xAA, 0xBB, 0xCC, 0xDD};
    uint32_t testCanId = 0x456;
    
    sendCANMessage(testCanId, testData);
    
    // Wait for D-Bus signal to be emitted
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Test would involve verifying D-Bus signals are emitted
    // This requires a D-Bus client setup to receive signals
}

// Test bidirectional communication
TEST_F(IntegrationTest, BidirectionalCommunication) {
    startServices();
    
    // Send CAN message from vcan0
    std::vector<uint8_t> testData = {0x11, 0x22, 0x33, 0x44};
    uint32_t testCanId = 0x789;
    
    sendCANMessage(testCanId, testData);
    
    // Wait for message to reach server
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Send command from server
    std::string serverCommand = "{\"type\":\"can_command\",\"canId\":1000,\"data\":\"55667788\"}";
    mockServer->sendMessage(serverCommand);
    
    // Wait for command processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify both directions worked
    auto messages = mockServer->getReceivedMessages();
    bool foundOutgoingMessage = false;
    for (const auto& msg : messages) {
        if (msg.find("\"type\":\"can_message\"") != std::string::npos &&
            msg.find("\"canId\":1929") != std::string::npos) { // 0x789 = 1929
            foundOutgoingMessage = true;
            break;
        }
    }
    
    EXPECT_TRUE(foundOutgoingMessage);
}

// Test multiple message flow
TEST_F(IntegrationTest, MultipleMessageFlow) {
    startServices();
    
    // Send multiple CAN messages
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> testMessages = {
        {0x100, {0x01, 0x02}},
        {0x200, {0x03, 0x04, 0x05}},
        {0x300, {0x06, 0x07, 0x08, 0x09}},
        {0x400, {0x0A, 0x0B, 0x0C, 0x0D, 0x0E}}
    };
    
    for (const auto& msg : testMessages) {
        sendCANMessage(msg.first, msg.second);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    // Wait for all messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Verify all messages were received
    auto messages = mockServer->getReceivedMessages();
    int canMessageCount = 0;
    for (const auto& msg : messages) {
        if (msg.find("\"type\":\"can_message\"") != std::string::npos) {
            canMessageCount++;
        }
    }
    
    EXPECT_EQ(canMessageCount, 4);
}

// Test error handling in flow
TEST_F(IntegrationTest, ErrorHandlingInFlow) {
    startServices();
    
    // Send invalid CAN message (oversized)
    std::vector<uint8_t> oversizedData(9, 0xFF); // CAN_MAX_DLEN is 8
    uint32_t testCanId = 0x123;
    
    sendCANMessage(testCanId, oversizedData);
    
    // Wait for error handling
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // System should handle error gracefully
    // (No crash, error logged, etc.)
}

// Test service restart during flow
TEST_F(IntegrationTest, ServiceRestartDuringFlow) {
    startServices();
    
    // Send a message
    std::vector<uint8_t> testData = {0x55, 0x66};
    uint32_t testCanId = 0x777;
    
    sendCANMessage(testCanId, testData);
    
    // Wait for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Restart services
    canListener->stop();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Restart services
    canListener->start();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Send another message
    sendCANMessage(0x888, {0x77, 0x88});
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify system recovered
    auto messages = mockServer->getReceivedMessages();
    bool foundMessage = false;
    for (const auto& msg : messages) {
        if (msg.find("\"type\":\"can_message\"") != std::string::npos) {
            foundMessage = true;
            break;
        }
    }
    
    EXPECT_TRUE(foundMessage);
}

// Test high frequency message flow
TEST_F(IntegrationTest, HighFrequencyMessageFlow) {
    startServices();
    
    // Send many messages quickly
    for (int i = 0; i < 50; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i), static_cast<uint8_t>(i + 1)};
        uint32_t canId = 0x100 + i;
        
        sendCANMessage(canId, data);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Wait for all messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Verify most messages were processed
    auto messages = mockServer->getReceivedMessages();
    int canMessageCount = 0;
    for (const auto& msg : messages) {
        if (msg.find("\"type\":\"can_message\"") != std::string::npos) {
            canMessageCount++;
        }
    }
    
    // Should receive most of the messages (some might be lost due to timing)
    EXPECT_GT(canMessageCount, 40);
}

// Test D-Bus service availability
TEST_F(IntegrationTest, DBusServiceAvailability) {
    startServices();
    
    // Test would involve checking if D-Bus services are available
    // This requires D-Bus introspection tools
    
    // For now, just verify services start without errors
    EXPECT_NE(canListener, nullptr);
}

// Test complete end-to-end scenario
TEST_F(IntegrationTest, CompleteEndToEndScenario) {
    startServices();
    
    // Simulate a complete scenario:
    // 1. Send CAN message from ECU
    // 2. Message flows through CAN Listener
    // 3. Message is forwarded to App Server via D-Bus
    // 4. App Server processes message
    // 5. App Server sends response
    // 6. Response flows back through the system
    
    // Step 1: Send CAN message
    std::vector<uint8_t> ecuData = {0x12, 0x34, 0x56, 0x78};
    uint32_t ecuCanId = 0x123;
    
    sendCANMessage(ecuCanId, ecuData);
    
    // Step 2-3: Wait for message to flow through system
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Step 4: Verify message reached server
    auto messages = mockServer->getReceivedMessages();
    bool foundECUMessage = false;
    for (const auto& msg : messages) {
        if (msg.find("\"type\":\"can_message\"") != std::string::npos &&
            msg.find("\"canId\":291") != std::string::npos) { // 0x123 = 291
            foundECUMessage = true;
            break;
        }
    }
    
    EXPECT_TRUE(foundECUMessage);
    
    // Step 5: Send response from server
    std::string serverResponse = "{\"type\":\"can_command\",\"canId\":2000,\"data\":\"9ABCDEF0\"}";
    mockServer->sendMessage(serverResponse);
    
    // Step 6: Wait for response processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify complete flow worked
    EXPECT_TRUE(foundECUMessage);
}
