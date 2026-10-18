#include "devices/Pump.h"

Pump::Pump(const std::string& id, const std::string& machineId)
    : Device(id, machineId, "pump"),
      active(false) {
}

void Pump::turnOn() {
    active = true;
}

void Pump::turnOff() {
    active = false;
}

bool Pump::isActive() const {
    return active;
}