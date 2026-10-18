#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <string>
#include <chrono>

struct Telemetry {
    std::string machineId;
    std::string deviceId;
    std::string sensorType;
    double value;
    std::string quality;
    std::string unit;
    std::chrono::system_clock::time_point timestamp;
};

#endif