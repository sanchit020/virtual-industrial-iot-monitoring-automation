#include <iostream>
#include <cassert>
#include <cmath>
#include <chrono>

#include "factory/Machine.h"
#include "automation/AutomationEngine.h"
#include "telemetry/Telemetry.h"
#include "telemetry/TelemetryValidator.h"
#include "telemetry/TelemetryProcessor.h"
#include "events/EventLogger.h"
#include "health/DeviceHealthMonitor.h"
#include "config/ConfigManager.h"

void testTelemetryValidator() {
    TelemetryValidator validator;
    auto now = std::chrono::system_clock::now();

    Telemetry validTemp{"m1", "s_temp", "temperature", 75.0, "good", "C", now};
    assert(validator.validate(validTemp));

    Telemetry validCur{"m1", "s_cur", "current", 600.0, "good", "A", now};
    assert(validator.validate(validCur));

    Telemetry validRpm{"m1", "s_rpm", "rpm", 1800.0, "good", "RPM", now};
    assert(validator.validate(validRpm));

    Telemetry validLoad{"m1", "s_load", "load", 60.0, "good", "%", now};
    assert(validator.validate(validLoad));

    Telemetry validCoolant{"m1", "s_flow", "coolant_flow", 10.0, "good", "L/min", now};
    assert(validator.validate(validCoolant));

    Telemetry validFan{"m1", "s_fan", "fan", 1.0, "good", "state", now};
    assert(validator.validate(validFan));

    Telemetry validPump{"m1", "s_pump", "pump", 0.0, "good", "state", now};
    assert(validator.validate(validPump));

    Telemetry validStatus{"m1", "s_status", "status", 2.0, "good", "state", now};
    assert(validator.validate(validStatus));

    Telemetry badQuality{"m1", "s_temp", "temperature", 75.0, "bad", "C", now};
    assert(!validator.validate(badQuality));

    Telemetry emptyMachine{"", "s_temp", "temperature", 75.0, "good", "C", now};
    assert(!validator.validate(emptyMachine));

    Telemetry badTemp{"m1", "s_temp", "temperature", 999.0, "good", "C", now};
    assert(!validator.validate(badTemp));

    Telemetry badStatus{"m1", "s_status", "status", 5.0, "good", "state", now};
    assert(!validator.validate(badStatus));

    std::cout << "[PASS] TelemetryValidator tests passed" << std::endl;
}

void testAutomationEngineRules() {
    AutomationEngine automation;
    EventLogger logger;

    Machine m("test_m", "Test Machine");

    m.update(1.0);
    automation.evaluate(m, logger);
    assert(!m.isFanOn());
    assert(!m.isPumpOn());
    assert(!m.isShutdown());

    while (m.getTemperature() < 70.0) {
        m.setLoad(100.0);
        m.update(1.0);
    }

    automation.evaluate(m, logger);
    assert(m.isFanOn());
    assert(!m.isPumpOn());

    while (m.getTemperature() < 80.0) {
        m.update(1.0);
    }

    automation.evaluate(m, logger);
    assert(m.isFanOn());
    assert(m.isPumpOn());

    m.setLoad(200.0);
    while (m.getTemperature() < 95.0) {
        m.update(1.0);
        automation.evaluate(m, logger);
    }

    assert(m.isShutdown());
    assert(!m.isFanOn());
    assert(!m.isPumpOn());

    std::cout << "[PASS] AutomationEngine rules tests passed" << std::endl;
}

void testMachinePhysicsAndFaults() {
    Machine m("test_phys", "Physics Machine");

    assert(m.getTemperature() == 25.0);
    assert(m.getLoad() == 60.0);
    assert(!m.hasFault());

    m.update(1.0);
    assert(m.getCurrent() == 600.0);
    assert(m.getRPM() == 1800.0);
    assert(m.getTemperature() > 25.0);

    double tempBefore = m.getTemperature();
    m.triggerFanFailure();
    assert(m.isFanFailed());
    assert(m.hasFault());

    m.setFan(true);
    m.update(1.0);
    double tempAfter = m.getTemperature();
    assert(tempAfter > tempBefore);

    m.triggerPumpFailure();
    assert(m.isPumpFailed());
    m.setPump(true);
    assert(m.getCoolantFlow() == 0.0);

    m.triggerOverload(250.0);
    assert(m.isOverloaded());
    assert(m.getLoad() == 250.0);

    m.clearFaults();
    assert(!m.isFanFailed());
    assert(!m.isPumpFailed());
    assert(!m.isOverloaded());

    std::cout << "[PASS] Machine physics and faults tests passed" << std::endl;
}

void testDeviceHealthMonitor() {
    DeviceHealthMonitor monitor(2);

    monitor.registerDevice("m1", "sensor_temp");
    monitor.registerDevice("m1", "sensor_rpm");

    monitor.recordHeartbeat("m1", "sensor_temp");
    assert(monitor.isDeviceOnline("m1", "sensor_temp"));
    assert(!monitor.isDeviceOnline("m1", "sensor_rpm"));

    auto missing = monitor.getMissingSensors("m1");
    assert(missing.size() == 1);
    assert(missing[0] == "sensor_rpm");

    std::cout << "[PASS] DeviceHealthMonitor tests passed" << std::endl;
}

void testConfigManager() {
    ConfigManager cfg;
    cfg.loadFromFile("config/factory.json");
    const auto& c = cfg.getConfig();

    assert(!c.machines.empty());
    assert(c.influxOrg == "athena");
    assert(c.influxBucket == "industrial_iot");
    assert(c.influxToken.empty());
    assert(c.fanThreshold == 70.0);
    assert(c.pumpThreshold == 80.0);
    assert(c.shutdownThreshold == 95.0);

    std::cout << "[PASS] ConfigManager tests passed" << std::endl;
}

void testMQTTCommandProcessing() {
    Machine m("machine1", "Machine A");
    assert(!m.isShutdown());

    m.setLoad(150.0);
    assert(m.getLoad() == 150.0);
    assert(m.getCurrent() == 1500.0);
    assert(m.getRPM() == 4500.0);

    m.setFan(true);
    assert(m.isFanOn());

    m.setPump(true);
    assert(m.isPumpOn());
    assert(m.getCoolantFlow() == 10.0);

    m.triggerPumpFailure();
    assert(m.isPumpFailed());
    assert(m.getCoolantFlow() == 0.0);

    m.clearFaults();
    assert(!m.isPumpFailed());

    m.triggerShutdown();
    assert(m.isShutdown());
    assert(m.getLoad() == 0.0);
    assert(!m.isFanOn());
    assert(!m.isPumpOn());

    std::cout << "[PASS] MQTT command processing logic tests passed" << std::endl;
}

int main() {
    std::cout << "Running Industrial IoT Automated Test Suite..." << std::endl;
    testTelemetryValidator();
    testAutomationEngineRules();
    testMachinePhysicsAndFaults();
    testDeviceHealthMonitor();
    testConfigManager();
    testMQTTCommandProcessing();
    std::cout << "\nALL AUTOMATED TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
