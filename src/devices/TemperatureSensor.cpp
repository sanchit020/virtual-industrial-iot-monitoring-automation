#include "devices/TemperatureSensor.h"

TemperatureSensor::TemperatureSensor(const std::string& id,
                                     const std::string& machineId)
    : Sensor(id, machineId, "temperature") {
}

void TemperatureSensor::update(double value) {
    Sensor::update(value);
}