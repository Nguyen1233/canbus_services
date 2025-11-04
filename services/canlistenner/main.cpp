#include "CANListener.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>

CANListener* g_canListener = nullptr;

void signalHandler(int signal)
{
    std::cout << "Received signal " << signal << " - shutting down..." << std::endl;
    if (g_canListener) {
        g_canListener->stop();
    }
    exit(0);
}

int main(int argc, char* argv[])
{
    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "Starting DMS CAN Service..." << std::endl;
    
    // Get CAN Listener instance
    g_canListener = CANListener::instance();
    
    // Start the service
    g_canListener->start();
    
    std::cout << "DMS CAN Service started successfully" << std::endl;
    
    // Keep the service running
    while (true) {
        sleep(1);
    }
    
    return 0;
}
