#ifndef SENSOR_H
#define SENSOR_H

#include "devices/Device.h"

class Sensor : public Device {
protected:
    double value;

public:
    Sensor(const std::string& id,
           const std::string& machineId,
           const std::string& type);

    virtual ~Sensor() = default;

    virtual void update(double value);

    double getValue() const;
};

#endif