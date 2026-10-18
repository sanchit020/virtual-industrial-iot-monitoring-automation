#include "devices/Sensor.h"

Sensor::Sensor(const std::string& id,
               const std::string& machineId,
               const std::string& type)
    : Device(id, machineId, type),
      value(0.0) {
}

void Sensor::update(double value) {
    this->value = value;
}

double Sensor::getValue() const {
    return value;
}