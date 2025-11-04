#include "CANConnector.h"
#include <iostream>
#include <errno.h>
#include <string.h>
#include <chrono>
#include <thread>

CANConnector::CANConnector(const std::string& interfaceName)
    : m_interfaceName(interfaceName)
    , m_socket(-1)
    , m_connected(false)
    , m_shouldStop(false)
{
    memset(&m_addr, 0, sizeof(m_addr));
    memset(&m_ifr, 0, sizeof(m_ifr));
}

CANConnector::~CANConnector()
{
    disconnect();
}

bool CANConnector::connect()
{
    if (m_connected) {
        return true;
    }

    if (!setupSocket()) {
        if (m_errorCallback) {
            m_errorCallback("Failed to setup CAN socket: " + std::string(strerror(errno)));
        }
        return false;
    }

    m_connected = true;
    m_shouldStop = false;
    
    // Start read thread
    m_readThread = std::make_unique<std::thread>(&CANConnector::readThreadFunction, this);
    
    if (m_statusCallback) {
        m_statusCallback(true);
    }
    
    std::cout << "Connected to CAN interface: " << m_interfaceName << std::endl;
    return true;
}

void CANConnector::disconnect()
{
    if (!m_connected) {
        return;
    }

    m_shouldStop = true;
    
    if (m_readThread && m_readThread->joinable()) {
        m_readThread->join();
    }
    
    cleanupSocket();
    m_connected = false;
    
    if (m_statusCallback) {
        m_statusCallback(false);
    }
    
    std::cout << "Disconnected from CAN interface: " << m_interfaceName << std::endl;
}

bool CANConnector::isConnected() const
{
    return m_connected;
}

bool CANConnector::sendMessage(uint32_t canId, const std::vector<uint8_t>& data)
{
    if (!m_connected || m_socket < 0) {
        if (m_errorCallback) {
            m_errorCallback("CAN socket not connected");
        }
        return false;
    }

    if (data.size() > CAN_MAX_DLEN) {
        if (m_errorCallback) {
            m_errorCallback("Data too large: " + std::to_string(data.size()) + " bytes (max: " + std::to_string(CAN_MAX_DLEN) + ")");
        }
        return false;
    }

    struct can_frame frame;
    frame.can_id = canId;
    frame.can_dlc = data.size();
    memcpy(frame.data, data.data(), data.size());

    std::lock_guard<std::mutex> lock(m_socketMutex);
    ssize_t bytesWritten = write(m_socket, &frame, sizeof(frame));
    if (bytesWritten != sizeof(frame)) {
        if (m_errorCallback) {
            m_errorCallback("Failed to send CAN message: " + std::string(strerror(errno)));
        }
        return false;
    }

    std::cout << "Sent CAN message - ID: 0x" << std::hex << canId << std::dec 
              << " Data: ";
    for (uint8_t byte : data) {
        printf("%02X ", byte);
    }
    std::cout << std::endl;
    
    return true;
}

void CANConnector::setInterfaceName(const std::string& interfaceName)
{
    if (m_interfaceName != interfaceName) {
        bool wasConnected = m_connected;
        if (wasConnected) {
            disconnect();
        }
        m_interfaceName = interfaceName;
        if (wasConnected) {
            connect();
        }
    }
}

std::string CANConnector::interfaceName() const
{
    return m_interfaceName;
}

void CANConnector::setMessageCallback(MessageCallback callback)
{
    m_messageCallback = callback;
}

void CANConnector::setStatusCallback(StatusCallback callback)
{
    m_statusCallback = callback;
}

void CANConnector::setErrorCallback(ErrorCallback callback)
{
    m_errorCallback = callback;
}

bool CANConnector::setupSocket()
{
    // Create socket
    m_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socket < 0) {
        return false;
    }

    // Get interface index
    strcpy(m_ifr.ifr_name, m_interfaceName.c_str());
    if (ioctl(m_socket, SIOCGIFINDEX, &m_ifr) < 0) {
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Bind socket
    m_addr.can_family = AF_CAN;
    m_addr.can_ifindex = m_ifr.ifr_ifindex;
    
    if (bind(m_socket, (struct sockaddr*)&m_addr, sizeof(m_addr)) < 0) {
        close(m_socket);
        m_socket = -1;
        return false;
    }

    return true;
}

void CANConnector::cleanupSocket()
{
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }
}

void CANConnector::readThreadFunction()
{
    struct can_frame frame;
    
    while (!m_shouldStop) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_socket, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int result = select(m_socket + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (result > 0 && FD_ISSET(m_socket, &readfds)) {
            std::lock_guard<std::mutex> lock(m_socketMutex);
            ssize_t bytesRead = read(m_socket, &frame, sizeof(frame));
            
            if (bytesRead == sizeof(frame)) {
                std::vector<uint8_t> data(frame.data, frame.data + frame.can_dlc);
                
                if (m_messageCallback) {
                    m_messageCallback(frame.can_id, data);
                }
                
                std::cout << "Received CAN message - ID: 0x" << std::hex << frame.can_id << std::dec 
                          << " Data: ";
                for (int i = 0; i < frame.can_dlc; i++) {
                    printf("%02X ", frame.data[i]);
                }
                std::cout << std::endl;
            } else if (bytesRead < 0) {
                if (m_errorCallback) {
                    m_errorCallback("Error reading CAN socket: " + std::string(strerror(errno)));
                }
                break;
            }
        } else if (result < 0) {
            if (m_errorCallback) {
                m_errorCallback("Select error: " + std::string(strerror(errno)));
            }
            break;
        }
    }
}
