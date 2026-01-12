#include "cap_lora868.h"

bool CapLoRa868::init()
{
    return false;
}

void CapLoRa868::update()
{
}

bool CapLoRa868::loraSendMsg(const std::string& msg)
{
    (void)msg;
    return false;
}

TinyGPSPlus* CapLoRa868::borrowGPS()
{
    return nullptr;
}

void CapLoRa868::returnGPS()
{
}
