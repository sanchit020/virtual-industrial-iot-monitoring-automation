#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include "devices/Sensor.h"

class TemperatureSensor : public Sensor {
public:
    TemperatureSensor(const std::string& id,
                      const std::string& machineId);

    void update(double value) override;
};

#endif