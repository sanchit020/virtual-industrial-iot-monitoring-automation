#include "devices/Fan.h"

Fan::Fan(const std::string& id, const std::string& machineId)
    : Device(id, machineId, "fan"),
      active(false) {
}

void Fan::turnOn() {
    active = true;
}

void Fan::turnOff() {
    active = false;
}

bool Fan::isActive() const {
    return active;
}