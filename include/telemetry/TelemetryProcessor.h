#ifndef TELEMETRY_PROCESSOR_H
#define TELEMETRY_PROCESSOR_H

#include "telemetry/Telemetry.h"
#include "telemetry/TelemetryValidator.h"

class TelemetryProcessor {
private:
    TelemetryValidator validator;

public:
    bool process(Telemetry& telemetry);
};

#endif