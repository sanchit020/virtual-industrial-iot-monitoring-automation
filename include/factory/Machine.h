#ifndef MACHINE_H
#define MACHINE_H

#include <string>

class Machine {
private:
    std::string id;
    std::string name;

    double temperature;
    double current;
    double load;
    double rpm;
    double coolantFlow;

    bool fanOn;
    bool pumpOn;
    bool shutdown;

    bool fanFailed;
    bool pumpFailed;
    bool sensorFailed;
    bool overloaded;

public:
    Machine(const std::string& id, const std::string& name);

    void update(double deltaTime);

    void setFan(bool state);
    void setPump(bool state);
    void setLoad(double value);
    void triggerShutdown();

    void triggerFanFailure();
    void triggerPumpFailure();
    void triggerSensorFailure();
    void triggerOverload(double overloadValue);
    void clearFaults();

    const std::string& getId() const;
    const std::string& getName() const;

    double getTemperature() const;
    double getCurrent() const;
    double getLoad() const;
    double getRPM() const;
    double getCoolantFlow() const;

    bool isFanOn() const;
    bool isPumpOn() const;
    bool isShutdown() const;

    bool isFanFailed() const;
    bool isPumpFailed() const;
    bool isSensorFailed() const;
    bool isOverloaded() const;
    bool hasFault() const;
};

#endif