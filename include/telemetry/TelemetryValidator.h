#ifndef TELEMETRY_VALIDATOR_H
#define TELEMETRY_VALIDATOR_H

#include "telemetry/Telemetry.h"

class TelemetryValidator {
public:
    bool validate(const Telemetry& telemetry) const;
};

#endif