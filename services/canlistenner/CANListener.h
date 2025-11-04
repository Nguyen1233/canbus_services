#ifndef CANLISTENER_H
#define CANLISTENER_H

#include "../lib/can/CANConnector.h"
#include <memory>
#include <vector>
#include <string>
#include <sdbus-c++/sdbus-c++.h>

class CANListener
{
public:
    static CANListener* instance();
    void start();
    void stop();
    
    ~CANListener();

private:
    CANListener();
    
    void setupDBusInterface();
    void onCANMessageReceived(uint32_t canId, const std::vector<uint8_t>& data);
    void forwardCANMessageToECU(uint32_t canId, const std::vector<uint8_t>& data);
    void processAppServerMessage(const std::string& message);
    
    std::unique_ptr<CANConnector> m_canConnector;
    std::unique_ptr<sdbus::IConnection> m_dbusConnection;
    std::unique_ptr<sdbus::IObject> m_dbusObject;
    std::unique_ptr<std::thread> m_dbusThread;
    
    // D-Bus interface constants
    static constexpr const char* SERVICE_NAME = "org.example.DMS.CAN";
    static constexpr const char* OBJECT_PATH = "/org/example/DMS/CANListener";
    static constexpr const char* INTERFACE_NAME = "org.example.DMS.CAN";
};

#endif // CANLISTENER_H
