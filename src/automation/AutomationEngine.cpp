#include "automation/AutomationEngine.h"

#include <sstream>
#include <iomanip>

AutomationEngine::AutomationEngine(double fanThresh, double pumpThresh, double shutdownThresh)
    : fanThreshold(fanThresh), pumpThreshold(pumpThresh), shutdownThreshold(shutdownThresh) {
}

void AutomationEngine::setThresholds(double fanThresh, double pumpThresh, double shutdownThresh) {
    fanThreshold = fanThresh;
    pumpThreshold = pumpThresh;
    shutdownThreshold = shutdownThresh;
}

double AutomationEngine::getFanThreshold() const {
    return fanThreshold;
}

double AutomationEngine::getPumpThreshold() const {
    return pumpThreshold;
}

double AutomationEngine::getShutdownThreshold() const {
    return shutdownThreshold;
}

void AutomationEngine::resetState(const std::string& machineId) {
    auto it = previousStates.find(machineId);
    if (it != previousStates.end()) {
        previousStates.erase(it);
    }
}

void AutomationEngine::evaluate(Machine& machine, EventLogger& logger) {
    const std::string& id = machine.getId();

    if (previousStates.find(id) == previousStates.end()) {
        previousStates[id] = MachineState{};
    }

    MachineState& prev = previousStates[id];

    if (machine.isShutdown()) {
        if (!prev.wasShutdown) {
            std::ostringstream oss;
            oss << "Machine in shutdown mode at "
                << std::fixed << std::setprecision(1)
                << machine.getTemperature() << " C";
            logger.log(id, "SHUTDOWN", oss.str(), EventSeverity::CRITICAL);
            prev.wasShutdown = true;
        }
        return;
    }

    if (prev.wasShutdown && !machine.isShutdown()) {
        logger.log(id, "RECOVERY_SYSTEM", "Machine resumed normal operational status", EventSeverity::INFO);
        prev.wasShutdown = false;
    }

    if (machine.isFanFailed() && !prev.fanWasFailed) {
        logger.log(id, "FAULT_FAN_FAILURE", "Fan actuator mechanical failure detected", EventSeverity::CRITICAL);
        prev.fanWasFailed = true;
    } else if (!machine.isFanFailed() && prev.fanWasFailed) {
        logger.log(id, "RECOVERY_FAN", "Fan actuator restored to operational condition", EventSeverity::INFO);
        prev.fanWasFailed = false;
    }

    if (machine.isPumpFailed() && !prev.pumpWasFailed) {
        logger.log(id, "FAULT_PUMP_FAILURE", "Coolant pump motor failure detected", EventSeverity::CRITICAL);
        prev.pumpWasFailed = true;
    } else if (!machine.isPumpFailed() && prev.pumpWasFailed) {
        logger.log(id, "RECOVERY_PUMP", "Coolant pump restored to operational condition", EventSeverity::INFO);
        prev.pumpWasFailed = false;
    }

    if (machine.isOverloaded() && !prev.wasOverloaded) {
        std::ostringstream oss;
        oss << "Machine overload condition: load at "
            << std::fixed << std::setprecision(1)
            << machine.getLoad() << "%";
        logger.log(id, "FAULT_OVERLOAD", oss.str(), EventSeverity::WARNING);
        prev.wasOverloaded = true;
    } else if (!machine.isOverloaded() && prev.wasOverloaded) {
        logger.log(id, "RECOVERY_OVERLOAD", "Machine load normalized within limits", EventSeverity::INFO);
        prev.wasOverloaded = false;
    }

    if (machine.isSensorFailed() && !prev.sensorWasFailed) {
        logger.log(id, "FAULT_SENSOR_FAILURE", "Temperature sensor invalid or offline (-999 C sentinel)", EventSeverity::WARNING);
        prev.sensorWasFailed = true;
    } else if (!machine.isSensorFailed() && prev.sensorWasFailed) {
        logger.log(id, "RECOVERY_SENSOR", "Temperature sensor restored and providing valid readings", EventSeverity::INFO);
        prev.sensorWasFailed = false;
    }

    if (machine.isSensorFailed()) {
        machine.setFan(true);
        if (!prev.fanWasOn) {
            logger.log(id, "FAN_ON", "Fail-safe cooling active due to sensor failure", EventSeverity::WARNING);
            prev.fanWasOn = true;
        }
        return;
    }

    double temperature = machine.getTemperature();

    if (temperature >= shutdownThreshold) {
        machine.triggerShutdown();
        std::ostringstream oss;
        oss << "Emergency shutdown triggered at "
            << std::fixed << std::setprecision(1)
            << temperature << " C (exceeded threshold " << shutdownThreshold << " C)";
        logger.log(id, "SHUTDOWN", oss.str(), EventSeverity::CRITICAL);
        prev.wasShutdown = true;
    } else if (temperature >= pumpThreshold) {
        machine.setPump(true);
        machine.setFan(true);

        if (!prev.pumpWasOn) {
            std::ostringstream oss;
            oss << "Pump activated at "
                << std::fixed << std::setprecision(1)
                << temperature << " C (threshold " << pumpThreshold << " C)";
            logger.log(id, "PUMP_ON", oss.str(), EventSeverity::WARNING);
            prev.pumpWasOn = true;
        }

        if (!prev.fanWasOn) {
            std::ostringstream oss;
            oss << "Fan activated at "
                << std::fixed << std::setprecision(1)
                << temperature << " C (threshold " << fanThreshold << " C)";
            logger.log(id, "FAN_ON", oss.str(), EventSeverity::INFO);
            prev.fanWasOn = true;
        }
    } else if (temperature >= fanThreshold) {
        machine.setFan(true);
        machine.setPump(false);

        if (!prev.fanWasOn) {
            std::ostringstream oss;
            oss << "Fan activated at "
                << std::fixed << std::setprecision(1)
                << temperature << " C (threshold " << fanThreshold << " C)";
            logger.log(id, "FAN_ON", oss.str(), EventSeverity::INFO);
            prev.fanWasOn = true;
        }

        if (prev.pumpWasOn) {
            logger.log(id, "PUMP_OFF", "Pump deactivated: temperature fell below pump threshold", EventSeverity::INFO);
            prev.pumpWasOn = false;
        }
    } else {
        machine.setFan(false);
        machine.setPump(false);

        if (prev.fanWasOn) {
            logger.log(id, "FAN_OFF", "Fan deactivated: temperature below fan threshold", EventSeverity::INFO);
            prev.fanWasOn = false;
        }

        if (prev.pumpWasOn) {
            logger.log(id, "PUMP_OFF", "Pump deactivated: temperature below fan threshold", EventSeverity::INFO);
            prev.pumpWasOn = false;
        }
    }
}