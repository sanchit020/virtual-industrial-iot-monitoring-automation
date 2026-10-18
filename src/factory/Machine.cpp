#include "factory/Machine.h"

Machine::Machine(const std::string& id, const std::string& name)
    : id(id),
      name(name),
      temperature(25.0),
      current(0.0),
      load(60.0),
      rpm(1800.0),
      coolantFlow(0.0),
      fanOn(false),
      pumpOn(false),
      shutdown(false),
      fanFailed(false),
      pumpFailed(false),
      sensorFailed(false),
      overloaded(false) {
}

void Machine::update(double deltaTime) {
    if (shutdown) {
        load = 0.0;
        current = 0.0;
        rpm = 0.0;
        fanOn = false;
        pumpOn = false;
        coolantFlow = 0.0;
        temperature -= 0.5 * deltaTime;

        if (temperature < 25.0) {
            temperature = 25.0;
        }

        return;
    }

    if (load > 100.0) {
        overloaded = true;
    }

    current = load * 10.0;
    rpm = load * 30.0;

    temperature += load * 0.02 * deltaTime;

    if (isFanRunning()) {
        temperature -= 1.0 * deltaTime;
    }

    if (isPumpRunning()) {
        temperature -= 2.5 * deltaTime;
        coolantFlow = 10.0;
    } else {
        coolantFlow = 0.0;
    }

    if (temperature < 25.0) {
        temperature = 25.0;
    }
}

void Machine::setFan(bool state) {
    if (!shutdown) {
        fanOn = state;
    }
}

void Machine::setPump(bool state) {
    if (!shutdown) {
        pumpOn = state;
        coolantFlow = isPumpRunning() ? 10.0 : 0.0;
    }
}

void Machine::setLoad(double value) {
    if (!shutdown) {
        load = value;
        current = load * 10.0;
        rpm = load * 30.0;
    }
}

void Machine::triggerShutdown() {
    shutdown = true;
    fanOn = false;
    pumpOn = false;
    coolantFlow = 0.0;
    load = 0.0;
    current = 0.0;
    rpm = 0.0;
}

void Machine::triggerFanFailure() {
    fanFailed = true;
}

void Machine::triggerPumpFailure() {
    pumpFailed = true;
    coolantFlow = 0.0;
}

void Machine::triggerSensorFailure() {
    sensorFailed = true;
}

void Machine::triggerOverload(double overloadValue) {
    if (!shutdown) {
        load = overloadValue;
        current = load * 10.0;
        rpm = load * 30.0;
        overloaded = true;
    }
}

void Machine::clearFaults() {
    fanFailed = false;
    pumpFailed = false;
    sensorFailed = false;
    overloaded = false;
}

bool Machine::attemptRecovery(std::string& rejectionReason) {
    if (shutdown) {
        if (temperature >= 80.0) {
            rejectionReason = "Recovery rejected: Core temperature " + std::to_string(static_cast<int>(temperature)) + " C is above safe threshold (80 C). Wait for cooldown.";
            return false;
        }
        shutdown = false;
        load = 60.0;
        current = 600.0;
        rpm = 1800.0;
    }

    if (load > 100.0) {
        load = 60.0;
        current = 600.0;
        rpm = 1800.0;
    }

    fanFailed = false;
    pumpFailed = false;
    sensorFailed = false;
    overloaded = false;
    rejectionReason = "Machine recovered to normal operational state";
    return true;
}

const std::string& Machine::getId() const {
    return id;
}

const std::string& Machine::getName() const {
    return name;
}

double Machine::getTemperature() const {
    return temperature;
}

double Machine::getCurrent() const {
    return current;
}

double Machine::getLoad() const {
    return load;
}

double Machine::getRPM() const {
    return rpm;
}

double Machine::getCoolantFlow() const {
    return coolantFlow;
}

bool Machine::isFanOn() const {
    return isFanRunning();
}

bool Machine::isPumpOn() const {
    return isPumpRunning();
}

bool Machine::isShutdown() const {
    return shutdown;
}

bool Machine::isFanCommanded() const {
    return fanOn;
}

bool Machine::isPumpCommanded() const {
    return pumpOn;
}

bool Machine::isFanOperational() const {
    return !fanFailed;
}

bool Machine::isPumpOperational() const {
    return !pumpFailed;
}

bool Machine::isFanRunning() const {
    return fanOn && !fanFailed && !shutdown;
}

bool Machine::isPumpRunning() const {
    return pumpOn && !pumpFailed && !shutdown;
}

double Machine::getFanCoolingRate() const {
    return isFanRunning() ? 1.0 : 0.0;
}

double Machine::getPumpCoolingRate() const {
    return isPumpRunning() ? 2.5 : 0.0;
}

int Machine::getFanTelemetryState() const {
    if (fanFailed) {
        return 2;
    }
    return fanOn ? 1 : 0;
}

int Machine::getPumpTelemetryState() const {
    if (pumpFailed) {
        return 2;
    }
    return pumpOn ? 1 : 0;
}

bool Machine::isFanFailed() const {
    return fanFailed;
}

bool Machine::isPumpFailed() const {
    return pumpFailed;
}

bool Machine::isSensorFailed() const {
    return sensorFailed;
}

bool Machine::isOverloaded() const {
    return overloaded;
}

bool Machine::hasFault() const {
    return fanFailed || pumpFailed || sensorFailed || overloaded;
}