#include "telemetry/TelemetryProcessor.h"

bool TelemetryProcessor::process(Telemetry& telemetry) {
    if (!validator.validate(telemetry)) {
        telemetry.quality = "invalid";
        return false;
    }

    return true;
}