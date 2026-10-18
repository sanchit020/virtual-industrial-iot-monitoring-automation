#ifndef PUMP_H
#define PUMP_H

#include "devices/Device.h"

class Pump : public Device {
private:
    bool active;

public:
    Pump(const std::string& id, const std::string& machineId);

    void turnOn();
    void turnOff();

    bool isActive() const;
};

#endif