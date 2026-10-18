#include "devices/RPMSensor.h"

RPMSensor::RPMSensor(const std::string& id,
                     const std::string& machineId)
    : Sensor(id, machineId, "rpm") {
}

void RPMSensor::update(double value) {
    Sensor::update(value);
}