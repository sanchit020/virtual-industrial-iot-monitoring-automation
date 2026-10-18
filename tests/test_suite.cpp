#include <iostream>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <vector>
#include <stdexcept>

#include "factory/Machine.h"
#include "automation/AutomationEngine.h"
#include "telemetry/Telemetry.h"
#include "telemetry/TelemetryValidator.h"
#include "telemetry/TelemetryProcessor.h"
#include "events/EventLogger.h"
#include "health/DeviceHealthMonitor.h"
#include "config/ConfigManager.h"

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "[TEST FAILED] Condition: (" #condition ") at " \
                      << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void testAssertionIntegrity() {
    bool caughtFailure = false;
    auto checkEvaluator = [](bool cond) {
        if (!cond) {
            throw std::runtime_error("Assertion detected intentional failure");
        }
    };

    try {
        checkEvaluator(1 == 2);
    } catch (const std::runtime_error&) {
        caughtFailure = true;
    }

    TEST_ASSERT(caughtFailure);
    TEST_ASSERT(2 + 2 == 4);
    std::cout << "[PASS] testAssertionIntegrity (Release-mode active verification)" << std::endl;
}

void testTelemetryValidator() {
    TelemetryValidator validator;
    auto now = std::chrono::system_clock::now();

    Telemetry validTemp{"m1", "s_temp", "temperature", 75.0, "good", "C", now};
    TEST_ASSERT(validator.validate(validTemp));

    Telemetry validCur{"m1", "s_cur", "current", 600.0, "good", "A", now};
    TEST_ASSERT(validator.validate(validCur));

    Telemetry validRpm{"m1", "s_rpm", "rpm", 1800.0, "good", "RPM", now};
    TEST_ASSERT(validator.validate(validRpm));

    Telemetry validLoad{"m1", "s_load", "load", 60.0, "good", "%", now};
    TEST_ASSERT(validator.validate(validLoad));

    Telemetry validCoolant{"m1", "s_flow", "coolant_flow", 10.0, "good", "L/min", now};
    TEST_ASSERT(validator.validate(validCoolant));

    Telemetry validFan{"m1", "s_fan", "fan", 1.0, "good", "state", now};
    TEST_ASSERT(validator.validate(validFan));

    Telemetry validFanFailedState{"m1", "s_fan", "fan", 2.0, "good", "state", now};
    TEST_ASSERT(validator.validate(validFanFailedState));

    Telemetry validPump{"m1", "s_pump", "pump", 0.0, "good", "state", now};
    TEST_ASSERT(validator.validate(validPump));

    Telemetry validPumpFailedState{"m1", "s_pump", "pump", 2.0, "good", "state", now};
    TEST_ASSERT(validator.validate(validPumpFailedState));

    Telemetry validStatus{"m1", "s_status", "status", 2.0, "good", "state", now};
    TEST_ASSERT(validator.validate(validStatus));

    Telemetry badQuality{"m1", "s_temp", "temperature", 75.0, "bad", "C", now};
    TEST_ASSERT(!validator.validate(badQuality));

    Telemetry emptyMachine{"", "s_temp", "temperature", 75.0, "good", "C", now};
    TEST_ASSERT(!validator.validate(emptyMachine));

    Telemetry sentinelTemp{"m1", "s_temp", "temperature", -999.0, "bad", "C", now};
    TEST_ASSERT(!validator.validate(sentinelTemp));

    Telemetry badStatus{"m1", "s_status", "status", 5.0, "good", "state", now};
    TEST_ASSERT(!validator.validate(badStatus));

    std::cout << "[PASS] testTelemetryValidator" << std::endl;
}

void testAutomationEngineRules() {
    AutomationEngine automation(70.0, 80.0, 95.0);
    EventLogger logger;

    Machine m("test_m", "Test Machine");

    m.update(1.0);
    automation.evaluate(m, logger);
    TEST_ASSERT(!m.isFanOn());
    TEST_ASSERT(!m.isPumpOn());
    TEST_ASSERT(!m.isShutdown());

    while (m.getTemperature() < 70.0) {
        m.setLoad(100.0);
        m.update(1.0);
    }

    automation.evaluate(m, logger);
    TEST_ASSERT(m.isFanOn());
    TEST_ASSERT(!m.isPumpOn());

    while (m.getTemperature() < 80.0) {
        m.update(1.0);
    }

    automation.evaluate(m, logger);
    TEST_ASSERT(m.isFanOn());
    TEST_ASSERT(m.isPumpOn());

    m.setLoad(200.0);
    while (m.getTemperature() < 95.0) {
        m.update(1.0);
        automation.evaluate(m, logger);
    }

    TEST_ASSERT(m.isShutdown());
    TEST_ASSERT(!m.isFanOn());
    TEST_ASSERT(!m.isPumpOn());
    TEST_ASSERT(m.getLoad() == 0.0);
    TEST_ASSERT(m.getCurrent() == 0.0);
    TEST_ASSERT(m.getRPM() == 0.0);

    Machine mSensor("test_sensor", "Sensor Test Machine");
    mSensor.triggerSensorFailure();
    automation.evaluate(mSensor, logger);
    TEST_ASSERT(mSensor.isFanCommanded());

    std::cout << "[PASS] testAutomationEngineRules" << std::endl;
}

void testMachinePhysicsAndFaults() {
    Machine m("test_phys", "Physics Machine");

    TEST_ASSERT(m.getTemperature() == 25.0);
    TEST_ASSERT(m.getLoad() == 60.0);
    TEST_ASSERT(!m.hasFault());

    m.update(1.0);
    TEST_ASSERT(m.getCurrent() == 600.0);
    TEST_ASSERT(m.getRPM() == 1800.0);
    TEST_ASSERT(m.getTemperature() > 25.0);

    m.triggerFanFailure();
    TEST_ASSERT(m.isFanFailed());
    TEST_ASSERT(!m.isFanOperational());
    m.setFan(true);
    TEST_ASSERT(m.isFanCommanded());
    TEST_ASSERT(!m.isFanRunning());
    TEST_ASSERT(m.getFanTelemetryState() == 2);
    TEST_ASSERT(m.getFanCoolingRate() == 0.0);

    m.triggerPumpFailure();
    TEST_ASSERT(m.isPumpFailed());
    TEST_ASSERT(!m.isPumpOperational());
    m.setPump(true);
    TEST_ASSERT(m.isPumpCommanded());
    TEST_ASSERT(!m.isPumpRunning());
    TEST_ASSERT(m.getPumpTelemetryState() == 2);
    TEST_ASSERT(m.getPumpCoolingRate() == 0.0);
    TEST_ASSERT(m.getCoolantFlow() == 0.0);

    m.triggerOverload(250.0);
    TEST_ASSERT(m.isOverloaded());
    TEST_ASSERT(m.getLoad() == 250.0);

    m.clearFaults();
    TEST_ASSERT(!m.isFanFailed());
    TEST_ASSERT(!m.isPumpFailed());
    TEST_ASSERT(!m.isOverloaded());
    TEST_ASSERT(m.isFanOperational());
    TEST_ASSERT(m.isPumpOperational());

    std::cout << "[PASS] testMachinePhysicsAndFaults" << std::endl;
}

void testMachineRecoveryRules() {
    Machine m("test_rec", "Recovery Test");

    while (m.getTemperature() < 85.0) {
        m.setLoad(150.0);
        m.update(1.0);
    }
    m.triggerShutdown();
    TEST_ASSERT(m.isShutdown());

    std::string rejectionReason;
    bool recovered = m.attemptRecovery(rejectionReason);
    TEST_ASSERT(!recovered);
    TEST_ASSERT(m.isShutdown());
    TEST_ASSERT(!rejectionReason.empty());

    while (m.getTemperature() >= 80.0) {
        m.update(1.0);
    }

    recovered = m.attemptRecovery(rejectionReason);
    TEST_ASSERT(recovered);
    TEST_ASSERT(!m.isShutdown());
    TEST_ASSERT(m.getLoad() == 60.0);

    std::cout << "[PASS] testMachineRecoveryRules (Safe cooldown verification)" << std::endl;
}

void testDeviceHealthMonitor() {
    DeviceHealthMonitor monitor(2);

    monitor.registerDevice("m1", "sensor_temp");
    monitor.registerDevice("m1", "actuator_fan");

    monitor.updateDeviceStatus("m1", "sensor_temp", true, false, true, "Sensor reading -999 C");
    TEST_ASSERT(monitor.isDeviceOnline("m1", "sensor_temp"));
    TEST_ASSERT(!monitor.isDeviceValid("m1", "sensor_temp"));
    TEST_ASSERT(!monitor.isDeviceHealthy("m1", "sensor_temp"));

    monitor.updateDeviceStatus("m1", "actuator_fan", true, true, false, "Fan mechanical failure");
    TEST_ASSERT(monitor.isDeviceOnline("m1", "actuator_fan"));
    TEST_ASSERT(!monitor.isDeviceOperational("m1", "actuator_fan"));
    TEST_ASSERT(!monitor.isDeviceHealthy("m1", "actuator_fan"));

    auto unhealthy = monitor.getUnhealthyDevices();
    TEST_ASSERT(unhealthy.size() == 2);

    monitor.updateDeviceStatus("m1", "sensor_temp", true, true, true, "Operational");
    TEST_ASSERT(monitor.isDeviceHealthy("m1", "sensor_temp"));

    std::cout << "[PASS] testDeviceHealthMonitor (Heartbeat vs validity vs operational state)" << std::endl;
}

void testConfigManager() {
    ConfigManager cfg;
    bool loaded = cfg.loadFromFile("config/factory.json");
    TEST_ASSERT(loaded);

    const auto& c = cfg.getConfig();
    TEST_ASSERT(!c.machines.empty());
    TEST_ASSERT(c.influxOrg == "athena");
    TEST_ASSERT(c.influxBucket == "industrial_iot");
    TEST_ASSERT(c.fanThreshold == 70.0);
    TEST_ASSERT(c.pumpThreshold == 80.0);
    TEST_ASSERT(c.shutdownThreshold == 95.0);

    size_t count1 = c.machines.size();
    cfg.loadFromFile("config/factory.json");
    TEST_ASSERT(cfg.getConfig().machines.size() == count1);

    FactoryConfig badConfig = c;
    badConfig.fanThreshold = 100.0;
    badConfig.pumpThreshold = 80.0;
    cfg.setConfig(badConfig);
    TEST_ASSERT(cfg.getConfig().fanThreshold == 70.0);
    TEST_ASSERT(cfg.getConfig().pumpThreshold == 80.0);

    std::cout << "[PASS] testConfigManager (JSON parsing, clearing on reload, threshold validation)" << std::endl;
}

void testMQTTCommandLogic() {
    Machine m("machine_cmd", "Command Test Machine");

    m.setLoad(150.0);
    TEST_ASSERT(m.getLoad() == 150.0);
    TEST_ASSERT(m.getCurrent() == 1500.0);
    TEST_ASSERT(m.getRPM() == 4500.0);

    m.setFan(true);
    TEST_ASSERT(m.isFanCommanded());
    TEST_ASSERT(m.isFanOperational());
    TEST_ASSERT(m.isFanRunning());
    TEST_ASSERT(m.getFanTelemetryState() == 1);

    m.triggerFanFailure();
    TEST_ASSERT(m.isFanCommanded());
    TEST_ASSERT(!m.isFanOperational());
    TEST_ASSERT(!m.isFanRunning());
    TEST_ASSERT(m.getFanTelemetryState() == 2);

    m.setPump(true);
    TEST_ASSERT(m.isPumpCommanded());
    TEST_ASSERT(m.isPumpRunning());
    TEST_ASSERT(m.getCoolantFlow() == 10.0);
    TEST_ASSERT(m.getPumpTelemetryState() == 1);

    m.triggerPumpFailure();
    TEST_ASSERT(!m.isPumpOperational());
    TEST_ASSERT(!m.isPumpRunning());
    TEST_ASSERT(m.getPumpTelemetryState() == 2);
    TEST_ASSERT(m.getCoolantFlow() == 0.0);

    m.triggerShutdown();
    TEST_ASSERT(m.isShutdown());
    TEST_ASSERT(m.getLoad() == 0.0);
    TEST_ASSERT(m.getCurrent() == 0.0);
    TEST_ASSERT(m.getRPM() == 0.0);
    TEST_ASSERT(!m.isFanRunning());
    TEST_ASSERT(!m.isPumpRunning());

    std::string rejection;
    bool recovered = m.attemptRecovery(rejection);
    TEST_ASSERT(recovered);
    TEST_ASSERT(!m.isShutdown());
    TEST_ASSERT(m.getLoad() == 60.0);

    std::cout << "[PASS] testMQTTCommandLogic (Command execution, actuator separation, and shutdown telemetry)" << std::endl;
}

int main() {
    std::cout << "======================================================" << std::endl;
    std::cout << "   Virtual Industrial IoT Automated Test Suite" << std::endl;
    std::cout << "   (Release-mode active assertion validation)" << std::endl;
    std::cout << "======================================================" << std::endl;

    testAssertionIntegrity();
    testTelemetryValidator();
    testAutomationEngineRules();
    testMachinePhysicsAndFaults();
    testMachineRecoveryRules();
    testDeviceHealthMonitor();
    testConfigManager();
    testMQTTCommandLogic();

    std::cout << "\nALL 8 AUTOMATED TEST SUITES PASSED SUCCESSFULLY (100% VALIDATED)!" << std::endl;
    return 0;
}
