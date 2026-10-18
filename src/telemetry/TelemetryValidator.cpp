#include "telemetry/TelemetryValidator.h"

#include <cmath>

bool TelemetryValidator::validate(const Telemetry& telemetry) const {
    if (telemetry.machineId.empty() ||
        telemetry.deviceId.empty() ||
        telemetry.sensorType.empty()) {
        return false;
    }

    if (telemetry.quality != "good" && telemetry.quality != "bad" && telemetry.quality != "invalid") {
        return false;
    }

    if (telemetry.quality == "bad" || telemetry.quality == "invalid") {
        return false;
    }

    if (std::isnan(telemetry.value) || std::isinf(telemetry.value)) {
        return false;
    }

    if (telemetry.sensorType == "temperature") {
        if (telemetry.value < -50.0 || telemetry.value > 500.0) {
            return false;
        }
    } else if (telemetry.sensorType == "current") {
        if (telemetry.value < 0.0 || telemetry.value > 50000.0) {
            return false;
        }
    } else if (telemetry.sensorType == "rpm") {
        if (telemetry.value < 0.0 || telemetry.value > 100000.0) {
            return false;
        }
    } else if (telemetry.sensorType == "load") {
        if (telemetry.value < 0.0 || telemetry.value > 1000.0) {
            return false;
        }
    } else if (telemetry.sensorType == "coolant_flow") {
        if (telemetry.value < 0.0 || telemetry.value > 1000.0) {
            return false;
        }
    } else if (telemetry.sensorType == "fan" ||
               telemetry.sensorType == "pump") {
        if (telemetry.value != 0.0 && telemetry.value != 1.0 && telemetry.value != 2.0) {
            return false;
        }
    } else if (telemetry.sensorType == "fan_commanded" ||
               telemetry.sensorType == "pump_commanded") {
        if (telemetry.value != 0.0 && telemetry.value != 1.0) {
            return false;
        }
    } else if (telemetry.sensorType == "fan_cooling" ||
               telemetry.sensorType == "pump_cooling") {
        if (telemetry.value < 0.0 || telemetry.value > 100.0) {
            return false;
        }
    } else if (telemetry.sensorType == "status") {
        if (telemetry.value != 0.0 &&
            telemetry.value != 1.0 &&
            telemetry.value != 2.0) {
            return false;
        }
    }

    return true;
}