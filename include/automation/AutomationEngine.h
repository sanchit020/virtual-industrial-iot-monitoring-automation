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
    };

    std::map<std::string, MachineState> previousStates;

public:
    void evaluate(Machine& machine, EventLogger& logger);
};

#endif