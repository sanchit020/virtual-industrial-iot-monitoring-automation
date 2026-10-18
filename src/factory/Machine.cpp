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

    if (fanOn && !fanFailed) {
        temperature -= 1.0 * deltaTime;
    }

    if (pumpOn && !pumpFailed) {
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
    }
}

void Machine::setLoad(double value) {
    if (!shutdown) {
        load = value;
    }
}

void Machine::triggerShutdown() {
    shutdown = true;
    fanOn = false;
    pumpOn = false;
}

void Machine::triggerFanFailure() {
    fanFailed = true;
}

void Machine::triggerPumpFailure() {
    pumpFailed = true;
}

void Machine::triggerSensorFailure() {
    sensorFailed = true;
}

void Machine::triggerOverload(double overloadValue) {
    if (!shutdown) {
        load = overloadValue;
        overloaded = true;
    }
}

void Machine::clearFaults() {
    fanFailed = false;
    pumpFailed = false;
    sensorFailed = false;
    overloaded = false;
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
    return fanOn;
}

bool Machine::isPumpOn() const {
    return pumpOn;
}

bool Machine::isShutdown() const {
    return shutdown;
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