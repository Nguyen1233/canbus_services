#ifndef CANCONNECTOR_H
#define CANCONNECTOR_H

#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

class CANConnector
{
public:
    using MessageCallback = std::function<void(uint32_t canId, const std::vector<uint8_t>& data)>;
    using StatusCallback = std::function<void(bool connected)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    explicit CANConnector(const std::string& interfaceName = "vcan0");
    ~CANConnector();

    bool connect();
    void disconnect();
    bool isConnected() const;
    
    // Send CAN message
    bool sendMessage(uint32_t canId, const std::vector<uint8_t>& data);
    
    // Set CAN interface name
    void setInterfaceName(const std::string& interfaceName);
    std::string interfaceName() const;

    // Set callbacks
    void setMessageCallback(MessageCallback callback);
    void setStatusCallback(StatusCallback callback);
    void setErrorCallback(ErrorCallback callback);

private:
    bool setupSocket();
    void cleanupSocket();
    void readThreadFunction();
    
    std::string m_interfaceName;
    int m_socket;
    std::atomic<bool> m_connected;
    std::atomic<bool> m_shouldStop;
    
    struct sockaddr_can m_addr;
    struct ifreq m_ifr;
    
    // Callbacks
    MessageCallback m_messageCallback;
    StatusCallback m_statusCallback;
    ErrorCallback m_errorCallback;
    
    // Threading
    std::unique_ptr<std::thread> m_readThread;
    std::mutex m_socketMutex;
};

#endif // CANCONNECTOR_H
