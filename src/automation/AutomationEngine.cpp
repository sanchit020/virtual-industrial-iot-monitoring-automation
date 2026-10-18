#include "automation/AutomationEngine.h"

#include <sstream>
#include <iomanip>

void AutomationEngine::evaluate(Machine& machine, EventLogger& logger) {
    if (machine.isShutdown()) {
        return;
    }

    const std::string& id = machine.getId();

    if (previousStates.find(id) == previousStates.end()) {
        previousStates[id] = MachineState{};
    }

    MachineState& prev = previousStates[id];
    double temperature = machine.getTemperature();

    if (machine.isFanFailed() && !prev.fanWasFailed) {
        logger.log(id, "FAULT_FAN_FAILURE",
                   "Fan has failed - cooling degraded",
                   EventSeverity::CRITICAL);
        prev.fanWasFailed = true;
    }

    if (machine.isPumpFailed() && !prev.pumpWasFailed) {
        logger.log(id, "FAULT_PUMP_FAILURE",
                   "Pump has failed - liquid cooling offline",
                   EventSeverity::CRITICAL);
        prev.pumpWasFailed = true;
    }

    if (machine.isOverloaded() && !prev.wasOverloaded) {
        std::ostringstream oss;
        oss << "Machine overloaded - load at "
            << std::fixed << std::setprecision(1)
            << machine.getLoad() << "%";
        logger.log(id, "FAULT_OVERLOAD", oss.str(),
                   EventSeverity::WARNING);
        prev.wasOverloaded = true;
    }

    if (machine.isSensorFailed()) {
        logger.log(id, "FAULT_SENSOR_FAILURE",
                   "Temperature sensor failure - reading unreliable",
                   EventSeverity::WARNING);
    }

    if (temperature >= 95.0) {
        machine.triggerShutdown();

        std::ostringstream oss;
        oss << "Emergency shutdown at "
            << std::fixed << std::setprecision(1)
            << temperature << " C";
        logger.log(id, "SHUTDOWN", oss.str(),
                   EventSeverity::CRITICAL);
        prev.wasShutdown = true;

    } else if (temperature >= 80.0) {
        machine.setPump(true);
        machine.setFan(true);

        if (!prev.pumpWasOn) {
            std::ostringstream oss;
            oss << "Pump activated at "
                << std::fixed << std::setprecision(1)
                << temperature << " C";
            logger.log(id, "PUMP_ON", oss.str(),
                       EventSeverity::WARNING);
            prev.pumpWasOn = true;
        }

        if (!prev.fanWasOn) {
            std::ostringstream oss;
            oss << "Fan activated at "
                << std::fixed << std::setprecision(1)
                << temperature << " C";
            logger.log(id, "FAN_ON", oss.str(),
                       EventSeverity::INFO);
            prev.fanWasOn = true;
        }

    } else if (temperature >= 70.0) {
        machine.setFan(true);
        machine.setPump(false);

        if (!prev.fanWasOn) {
            std::ostringstream oss;
            oss << "Fan activated at "
                << std::fixed << std::setprecision(1)
                << temperature << " C";
            logger.log(id, "FAN_ON", oss.str(),
                       EventSeverity::INFO);
            prev.fanWasOn = true;
        }

        if (prev.pumpWasOn) {
            logger.log(id, "PUMP_OFF",
                       "Pump deactivated - temperature below 80 C",
                       EventSeverity::INFO);
            prev.pumpWasOn = false;
        }

    } else {
        machine.setFan(false);
        machine.setPump(false);

        if (prev.fanWasOn) {
            logger.log(id, "FAN_OFF",
                       "Fan deactivated - temperature below 70 C",
                       EventSeverity::INFO);
            prev.fanWasOn = false;
        }

        if (prev.pumpWasOn) {
            logger.log(id, "PUMP_OFF",
                       "Pump deactivated - temperature below 70 C",
                       EventSeverity::INFO);
            prev.pumpWasOn = false;
        }
    }
}