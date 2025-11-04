#include <gtest/gtest.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include "../lib/can/CANConnector.h"

class CANConnectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup vcan0 interface if it doesn't exist
        setupVCANInterface();
        
        // Create CAN connector instance
        canConnector = std::make_unique<CANConnector>("vcan0");
        
        // Reset test state
        messageReceived = false;
        statusChanged = false;
        errorOccurred = false;
        receivedCanId = 0;
        receivedData.clear();
        connectionStatus = false;
        lastError.clear();
    }

    void TearDown() override {
        if (canConnector) {
            canConnector->disconnect();
        }
        canConnector.reset();
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

    void setupCallbacks() {
        canConnector->setMessageCallback([this](uint32_t canId, const std::vector<uint8_t>& data) {
            messageReceived = true;
            receivedCanId = canId;
            receivedData = data;
        });

        canConnector->setStatusCallback([this](bool connected) {
            statusChanged = true;
            connectionStatus = connected;
        });

        canConnector->setErrorCallback([this](const std::string& error) {
            errorOccurred = true;
            lastError = error;
        });
    }

    std::unique_ptr<CANConnector> canConnector;
    std::atomic<bool> messageReceived;
    std::atomic<bool> statusChanged;
    std::atomic<bool> errorOccurred;
    std::atomic<uint32_t> receivedCanId;
    std::vector<uint8_t> receivedData;
    std::atomic<bool> connectionStatus;
    std::string lastError;
};

// Test basic connection
TEST_F(CANConnectorTest, BasicConnection) {
    setupCallbacks();
    
    EXPECT_FALSE(canConnector->isConnected());
    
    bool connected = canConnector->connect();
    EXPECT_TRUE(connected);
    EXPECT_TRUE(canConnector->isConnected());
    
    // Wait for status callback
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(statusChanged);
    EXPECT_TRUE(connectionStatus);
}

// Test disconnection
TEST_F(CANConnectorTest, Disconnection) {
    setupCallbacks();
    
    ASSERT_TRUE(canConnector->connect());
    EXPECT_TRUE(canConnector->isConnected());
    
    canConnector->disconnect();
    EXPECT_FALSE(canConnector->isConnected());
    
    // Wait for status callback
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(statusChanged);
    EXPECT_FALSE(connectionStatus);
}

// Test sending messages
TEST_F(CANConnectorTest, SendMessage) {
    setupCallbacks();
    ASSERT_TRUE(canConnector->connect());
    
    std::vector<uint8_t> testData = {0x01, 0x02, 0x03, 0x04};
    uint32_t testCanId = 0x123;
    
    bool sent = canConnector->sendMessage(testCanId, testData);
    EXPECT_TRUE(sent);
    
    // Wait a bit for message to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// Test receiving messages
TEST_F(CANConnectorTest, ReceiveMessage) {
    setupCallbacks();
    ASSERT_TRUE(canConnector->connect());
    
    // Create a separate socket to send test message
    int testSocket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    ASSERT_GE(testSocket, 0);
    
    struct ifreq ifr;
    strcpy(ifr.ifr_name, "vcan0");
    ASSERT_GE(ioctl(testSocket, SIOCGIFINDEX, &ifr), 0);
    
    struct sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    ASSERT_GE(bind(testSocket, (struct sockaddr*)&addr, sizeof(addr)), 0);
    
    // Send test message
    struct can_frame frame;
    frame.can_id = 0x456;
    frame.can_dlc = 4;
    frame.data[0] = 0xAA;
    frame.data[1] = 0xBB;
    frame.data[2] = 0xCC;
    frame.data[3] = 0xDD;
    
    ssize_t bytesWritten = write(testSocket, &frame, sizeof(frame));
    EXPECT_EQ(bytesWritten, sizeof(frame));
    
    // Wait for message to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_TRUE(messageReceived);
    EXPECT_EQ(receivedCanId, 0x456);
    EXPECT_EQ(receivedData.size(), 4);
    EXPECT_EQ(receivedData[0], 0xAA);
    EXPECT_EQ(receivedData[1], 0xBB);
    EXPECT_EQ(receivedData[2], 0xCC);
    EXPECT_EQ(receivedData[3], 0xDD);
    
    close(testSocket);
}

// Test interface name setting
TEST_F(CANConnectorTest, InterfaceName) {
    EXPECT_EQ(canConnector->interfaceName(), "vcan0");
    
    canConnector->setInterfaceName("vcan1");
    EXPECT_EQ(canConnector->interfaceName(), "vcan1");
}

// Test error handling for invalid interface
TEST_F(CANConnectorTest, InvalidInterface) {
    setupCallbacks();
    
    canConnector->setInterfaceName("invalid_interface");
    
    bool connected = canConnector->connect();
    EXPECT_FALSE(connected);
    EXPECT_FALSE(canConnector->isConnected());
    
    // Wait for error callback
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(errorOccurred);
    EXPECT_FALSE(lastError.empty());
}

// Test sending message when not connected
TEST_F(CANConnectorTest, SendMessageWhenDisconnected) {
    setupCallbacks();
    
    std::vector<uint8_t> testData = {0x01, 0x02};
    uint32_t testCanId = 0x123;
    
    bool sent = canConnector->sendMessage(testCanId, testData);
    EXPECT_FALSE(sent);
    
    // Wait for error callback
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(errorOccurred);
    EXPECT_FALSE(lastError.empty());
}

// Test sending oversized message
TEST_F(CANConnectorTest, SendOversizedMessage) {
    setupCallbacks();
    ASSERT_TRUE(canConnector->connect());
    
    // Create oversized data (CAN_MAX_DLEN is 8)
    std::vector<uint8_t> oversizedData(9, 0xFF);
    uint32_t testCanId = 0x123;
    
    bool sent = canConnector->sendMessage(testCanId, oversizedData);
    EXPECT_FALSE(sent);
    
    // Wait for error callback
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(errorOccurred);
    EXPECT_FALSE(lastError.empty());
}

// Test multiple message sending and receiving
TEST_F(CANConnectorTest, MultipleMessages) {
    setupCallbacks();
    ASSERT_TRUE(canConnector->connect());
    
    // Create test socket for sending
    int testSocket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    ASSERT_GE(testSocket, 0);
    
    struct ifreq ifr;
    strcpy(ifr.ifr_name, "vcan0");
    ASSERT_GE(ioctl(testSocket, SIOCGIFINDEX, &ifr), 0);
    
    struct sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    ASSERT_GE(bind(testSocket, (struct sockaddr*)&addr, sizeof(addr)), 0);
    
    // Send multiple messages
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> testMessages = {
        {0x100, {0x01, 0x02}},
        {0x200, {0x03, 0x04, 0x05}},
        {0x300, {0x06, 0x07, 0x08, 0x09}}
    };
    
    for (const auto& msg : testMessages) {
        struct can_frame frame;
        frame.can_id = msg.first;
        frame.can_dlc = msg.second.size();
        memcpy(frame.data, msg.second.data(), msg.second.size());
        
        ssize_t bytesWritten = write(testSocket, &frame, sizeof(frame));
        EXPECT_EQ(bytesWritten, sizeof(frame));
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Wait for all messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Verify last received message
    EXPECT_TRUE(messageReceived);
    EXPECT_EQ(receivedCanId, 0x300);
    EXPECT_EQ(receivedData.size(), 4);
    EXPECT_EQ(receivedData[0], 0x06);
    EXPECT_EQ(receivedData[1], 0x07);
    EXPECT_EQ(receivedData[2], 0x08);
    EXPECT_EQ(receivedData[3], 0x09);
    
    close(testSocket);
}

// Test reconnection
TEST_F(CANConnectorTest, Reconnection) {
    setupCallbacks();
    
    // First connection
    ASSERT_TRUE(canConnector->connect());
    EXPECT_TRUE(canConnector->isConnected());
    
    // Disconnect
    canConnector->disconnect();
    EXPECT_FALSE(canConnector->isConnected());
    
    // Reconnect
    ASSERT_TRUE(canConnector->connect());
    EXPECT_TRUE(canConnector->isConnected());
}

// Test thread safety
TEST_F(CANConnectorTest, ThreadSafety) {
    setupCallbacks();
    ASSERT_TRUE(canConnector->connect());

    std::atomic<int> sendCount{0};

    // Create a separate socket for sending messages (simulate external source)
    int testSocket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    ASSERT_GE(testSocket, 0);

    struct ifreq ifr;
    strcpy(ifr.ifr_name, "vcan0");
    ASSERT_GE(ioctl(testSocket, SIOCGIFINDEX, &ifr), 0);

    struct sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    ASSERT_GE(bind(testSocket, (struct sockaddr*)&addr, sizeof(addr)), 0);

    // Multiple threads sending messages via the test socket
    std::vector<std::thread> sendThreads;
    for (int i = 0; i < 5; ++i) {
        sendThreads.emplace_back([&sendCount, i, testSocket]() {
            for (int j = 0; j < 10; ++j) {
                struct can_frame frame;
                frame.can_id = 0x100 + i;
                frame.can_dlc = 2;
                frame.data[0] = static_cast<uint8_t>(i);
                frame.data[1] = static_cast<uint8_t>(j);
                ssize_t bytesWritten = write(testSocket, &frame, sizeof(frame));
                if (bytesWritten == sizeof(frame)) {
                    sendCount++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Wait for all send threads
    for (auto& thread : sendThreads) {
        thread.join();
    }

    // Wait for messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    close(testSocket);

    EXPECT_GT(sendCount.load(), 0);
    EXPECT_TRUE(messageReceived);
}
