#include "devices/LoadSensor.h"

LoadSensor::LoadSensor(const std::string& id,
                       const std::string& machineId)
    : Sensor(id, machineId, "load") {
}

void LoadSensor::update(double value) {
    Sensor::update(value);
}