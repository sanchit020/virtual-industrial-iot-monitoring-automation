#ifndef AUTOMATION_ENGINE_H
#define AUTOMATION_ENGINE_H

#include <map>
#include <string>
#include "factory/Machine.h"
#include "events/EventLogger.h"

class AutomationEngine {
private:
    struct MachineState {
        bool fanWasOn = false;
        bool pumpWasOn = false;
        bool wasShutdown = false;
        bool wasOverloaded = false;
        bool fanWasFailed = false;
        bool pumpWasFailed = false;
        bool sensorWasFailed = false;
    };

    double fanThreshold = 70.0;
    double pumpThreshold = 80.0;
    double shutdownThreshold = 95.0;

    std::map<std::string, MachineState> previousStates;

public:
    AutomationEngine() = default;
    AutomationEngine(double fanThresh, double pumpThresh, double shutdownThresh);

    void setThresholds(double fanThresh, double pumpThresh, double shutdownThresh);
    double getFanThreshold() const;
    double getPumpThreshold() const;
    double getShutdownThreshold() const;

    void evaluate(Machine& machine, EventLogger& logger);
    void resetState(const std::string& machineId);
};

#endif