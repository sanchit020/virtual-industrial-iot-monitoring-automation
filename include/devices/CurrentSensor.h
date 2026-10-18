#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include "devices/Sensor.h"

class CurrentSensor : public Sensor {
public:
    CurrentSensor(const std::string& id,
                  const std::string& machineId);

    void update(double value) override;
};

#endif