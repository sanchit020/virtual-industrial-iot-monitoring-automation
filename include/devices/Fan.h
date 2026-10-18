#ifndef FAN_H
#define FAN_H

#include "devices/Device.h"

class Fan : public Device {
private:
    bool active;

public:
    Fan(const std::string& id, const std::string& machineId);

    void turnOn();
    void turnOff();

    bool isActive() const;
};

#endif