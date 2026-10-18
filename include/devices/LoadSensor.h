#ifndef LOAD_SENSOR_H
#define LOAD_SENSOR_H

#include "devices/Sensor.h"

class LoadSensor : public Sensor {
public:
    LoadSensor(const std::string& id,
               const std::string& machineId);

    void update(double value) override;
};

#endif