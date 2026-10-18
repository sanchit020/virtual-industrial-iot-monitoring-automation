#include "devices/CurrentSensor.h"

CurrentSensor::CurrentSensor(const std::string& id,
                             const std::string& machineId)
    : Sensor(id, machineId, "current") {
}

void CurrentSensor::update(double value) {
    Sensor::update(value);
}