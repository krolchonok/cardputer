#pragma once
#include <BleMouse.h>
#include <string>

class BleMouseWrapper {
public:
    BleMouseWrapper(const std::string& deviceName = "CardputerMouse");
    void begin();
    void end();
    bool isConnected();
    void press(uint8_t button);
    void release(uint8_t button);
    void move(int x, int y);
    const std::string& getDeviceName() const;
    
    // Clear all BLE bonding data to fix pairing issues
    static void clearBondingData();

private:
    BleMouse _bleMouse;
    std::string _deviceName;
};