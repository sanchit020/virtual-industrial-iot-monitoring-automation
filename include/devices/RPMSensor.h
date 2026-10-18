#ifndef RPM_SENSOR_H
#define RPM_SENSOR_H

#include "devices/Sensor.h"

class RPMSensor : public Sensor {
public:
    RPMSensor(const std::string& id,
              const std::string& machineId);

    void update(double value) override;
};

#endif