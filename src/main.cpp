#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

#include "factory/Factory.h"
#include "automation/AutomationEngine.h"
#include "mqtt/MQTTClient.h"
#include "storage/InfluxWriter.h"
#include "telemetry/Telemetry.h"
#include "telemetry/TelemetryProcessor.h"
#include "events/EventLogger.h"
#include "health/DeviceHealthMonitor.h"
#include "config/ConfigManager.h"

namespace {
std::string getEnvOrDefault(const char* varName, const std::string& defaultValue) {
    const char* val = std::getenv(varName);
    if (val != nullptr && std::string(val).length() > 0) {
        return std::string(val);
    }
    return defaultValue;
}

void handleCommand(const std::string& topic,
                   const std::string& message,
                   Factory& factory,
                   MQTTClient& mqtt,
                   EventLogger& logger) {
    std::cout << "[MQTT-CMD] Received on " << topic << ": " << message << std::endl;

    std::string machineId = "";
    size_t firstSlash = topic.find('/');
    if (firstSlash != std::string::npos) {
        size_t secondSlash = topic.find('/', firstSlash + 1);
        if (secondSlash != std::string::npos) {
            machineId = topic.substr(firstSlash + 1, secondSlash - firstSlash - 1);
        }
    }

    if (machineId.empty()) {
        machineId = "machine1";
    }

    Machine* machine = factory.getMachine(machineId);
    std::string ackTopic = "factory/" + machineId + "/command/ack";

    if (!machine) {
        std::string nack = "{\"status\":\"ERROR\",\"message\":\"Machine not found: " + machineId + "\"}";
        mqtt.publish(ackTopic, nack);
        logger.log(machineId, "COMMAND_REJECTED", "Unknown machine: " + machineId, EventSeverity::WARNING);
        return;
    }

    std::string cmd = message;
    std::string valStr = "";
    size_t spacePos = message.find(' ');
    if (spacePos != std::string::npos) {
        cmd = message.substr(0, spacePos);
        valStr = message.substr(spacePos + 1);
    }

    bool success = false;
    std::string responseMsg = "";

    if (cmd == "STOP" || cmd == "SHUTDOWN" || message.find("\"STOP\"") != std::string::npos) {
        machine->triggerShutdown();
        success = true;
        responseMsg = "Shutdown command executed";
        logger.log(machineId, "COMMAND_EXECUTED", responseMsg, EventSeverity::CRITICAL);
    } else if (cmd == "SET_LOAD") {
        try {
            double loadVal = std::stod(valStr);
            if (loadVal >= 0.0 && loadVal <= 300.0) {
                machine->setLoad(loadVal);
                success = true;
                responseMsg = "Load set to " + std::to_string(loadVal) + "%";
                logger.log(machineId, "COMMAND_EXECUTED", responseMsg, EventSeverity::INFO);
            } else {
                responseMsg = "Load value out of bounds (0-300)";
            }
        } catch (...) {
            responseMsg = "Invalid load numeric format";
        }
    } else if (cmd == "SET_FAN") {
        if (valStr == "ON" || valStr == "1" || valStr == "true") {
            machine->setFan(true);
            success = true;
            responseMsg = "Fan turned ON";
        } else if (valStr == "OFF" || valStr == "0" || valStr == "false") {
            machine->setFan(false);
            success = true;
            responseMsg = "Fan turned OFF";
        } else {
            responseMsg = "Invalid fan state (use ON/OFF)";
        }
        if (success) {
            logger.log(machineId, "COMMAND_EXECUTED", responseMsg, EventSeverity::INFO);
        }
    } else if (cmd == "SET_PUMP") {
        if (valStr == "ON" || valStr == "1" || valStr == "true") {
            machine->setPump(true);
            success = true;
            responseMsg = "Pump turned ON";
        } else if (valStr == "OFF" || valStr == "0" || valStr == "false") {
            machine->setPump(false);
            success = true;
            responseMsg = "Pump turned OFF";
        } else {
            responseMsg = "Invalid pump state (use ON/OFF)";
        }
        if (success) {
            logger.log(machineId, "COMMAND_EXECUTED", responseMsg, EventSeverity::INFO);
        }
    } else if (cmd == "RESET_FAULT" || cmd == "CLEAR_FAULTS") {
        machine->clearFaults();
        success = true;
        responseMsg = "Faults cleared";
        logger.log(machineId, "COMMAND_EXECUTED", responseMsg, EventSeverity::INFO);
    } else if (cmd == "INJECT_FAULT") {
        if (valStr == "FAN") {
            machine->triggerFanFailure();
            success = true;
            responseMsg = "Fan failure injected";
        } else if (valStr == "PUMP") {
            machine->triggerPumpFailure();
            success = true;
            responseMsg = "Pump failure injected";
        } else if (valStr == "SENSOR") {
            machine->triggerSensorFailure();
            success = true;
            responseMsg = "Sensor failure injected";
        } else if (valStr == "OVERLOAD") {
            machine->triggerOverload(200.0);
            success = true;
            responseMsg = "Overload injected (200%)";
        } else {
            responseMsg = "Unknown fault type (use FAN, PUMP, SENSOR, OVERLOAD)";
        }
        if (success) {
            logger.log(machineId, "FAULT_INJECTED", responseMsg, EventSeverity::WARNING);
        }
    } else {
        responseMsg = "Unrecognized command: " + cmd;
    }

    std::string ackPayload = "{\"machine\":\"" + machineId + "\",\"command\":\"" + cmd + "\",\"status\":\"" + (success ? "SUCCESS" : "FAILED") + "\",\"message\":\"" + responseMsg + "\"}";
    mqtt.publish(ackTopic, ackPayload);
    std::cout << "[MQTT-ACK] " << ackTopic << " -> " << ackPayload << std::endl;
}
}

int main() {
    ConfigManager configMgr;
    configMgr.loadFromFile("config/factory.json");
    const FactoryConfig& config = configMgr.getConfig();

    std::cout << "======================================================" << std::endl;
    std::cout << "   " << config.factoryName << std::endl;
    std::cout << "   Virtual Industrial IoT Monitoring & Automation Platform" << std::endl;
    std::cout << "======================================================" << std::endl;

    Factory factory;
    for (const auto& m : config.machines) {
        factory.addMachine(m.id, m.name);
        std::cout << "[FACTORY] Registered machine: " << m.id << " (" << m.name << ")" << std::endl;
    }

    DeviceHealthMonitor healthMonitor(10);
    const std::vector<std::string> sensorTypes = {
        "temperature_sensor",
        "current_sensor",
        "rpm_sensor",
        "load_sensor",
        "coolant_flow_sensor",
        "fan_status",
        "pump_status",
        "machine_status"
    };

    for (const auto& m : config.machines) {
        for (const auto& st : sensorTypes) {
            healthMonitor.registerDevice(m.id, st);
        }
    }

    AutomationEngine automation;
    EventLogger eventLogger;
    TelemetryProcessor telemetryProcessor;

    MQTTClient mqtt(config.mqttBroker, config.mqttPort);
    bool mqttConnected = mqtt.connect();
    if (!mqttConnected) {
        std::cerr << "[MQTT-WARN] Could not connect to MQTT broker at "
                  << config.mqttBroker << ":" << config.mqttPort
                  << ". Telemetry will continue simulation without MQTT." << std::endl;
    } else {
        for (const auto& m : config.machines) {
            std::string cmdTopic = "factory/" + m.id + "/command";
            mqtt.subscribe(cmdTopic, [&factory, &mqtt, &eventLogger](const std::string& t, const std::string& msg) {
                handleCommand(t, msg, factory, mqtt, eventLogger);
            });
        }
        mqtt.subscribe("factory/all/command", [&factory, &mqtt, &eventLogger](const std::string& t, const std::string& msg) {
            handleCommand(t, msg, factory, mqtt, eventLogger);
        });
    }

    std::string influxUrl = getEnvOrDefault("INFLUX_URL", config.influxUrl);
    std::string influxOrg = getEnvOrDefault("INFLUX_ORG", config.influxOrg);
    std::string influxBucket = getEnvOrDefault("INFLUX_BUCKET", config.influxBucket);
    std::string influxToken = getEnvOrDefault("INFLUX_TOKEN", config.influxToken);

    std::cout << "[INFLUX] InfluxDB target: " << influxUrl
              << " | Org: " << influxOrg
              << " | Bucket: " << influxBucket << std::endl;

    if (influxToken.empty()) {
        std::cerr << "[INFLUX-WARN] No InfluxDB token detected. Set the INFLUX_TOKEN environment variable." << std::endl;
    }

    InfluxWriter influx(influxUrl, influxOrg, influxBucket, influxToken);

    eventLogger.log("factory", "SYSTEM_START", "Virtual Industrial IoT Platform started", EventSeverity::INFO);

    int elapsedSeconds = 0;
    const int maxSimulationSeconds = 50;
    size_t loggedEventsIndex = 0;

    while (elapsedSeconds <= maxSimulationSeconds) {
        std::cout << "\n--- [Cycle " << elapsedSeconds << "s] ----------------------------------------" << std::endl;

        if (elapsedSeconds == 15) {
            Machine* mb = factory.getMachine("machine2");
            if (mb) {
                mb->triggerFanFailure();
                eventLogger.log("machine2", "FAULT_INJECTED", "Simulated fan mechanical failure", EventSeverity::CRITICAL);
            }
        }

        if (elapsedSeconds == 20) {
            Machine* ma = factory.getMachine("machine1");
            if (ma) {
                ma->triggerOverload(200.0);
                eventLogger.log("machine1", "FAULT_INJECTED", "Simulated electrical overload (200% load)", EventSeverity::CRITICAL);
            }
        }

        if (elapsedSeconds == 25) {
            Machine* mc = factory.getMachine("machine3");
            if (mc) {
                mc->triggerSensorFailure();
                eventLogger.log("machine3", "FAULT_INJECTED", "Simulated temperature sensor failure", EventSeverity::WARNING);
            }
        }

        if (elapsedSeconds == 30) {
            Machine* mc = factory.getMachine("machine3");
            if (mc) {
                mc->triggerPumpFailure();
                mc->setLoad(150.0);
                eventLogger.log("machine3", "FAULT_INJECTED", "Simulated coolant pump motor failure with high load", EventSeverity::CRITICAL);
            }
        }

        bool anyActive = false;
        std::vector<Telemetry> cycleBatch;

        for (const auto& machinePtr : factory.getMachines()) {
            Machine* machine = machinePtr.get();
            const std::string& mId = machine->getId();
            const std::string& mName = machine->getName();

            machine->update(1.0);
            automation.evaluate(*machine, eventLogger);

            if (!machine->isShutdown()) {
                anyActive = true;
            }

            int statusValue;
            std::string statusText;
            if (machine->isShutdown()) {
                statusValue = 2;
                statusText = "SHUTDOWN";
            } else if (machine->getTemperature() >= 80.0 || machine->hasFault()) {
                statusValue = 1;
                statusText = "WARNING";
            } else {
                statusValue = 0;
                statusText = "RUNNING";
            }

            std::cout << std::left << std::setw(12) << mName
                      << " | Temp: " << std::fixed << std::setprecision(1) << std::setw(5) << machine->getTemperature() << " C"
                      << " | Load: " << std::setw(5) << machine->getLoad() << " %"
                      << " | Cur: " << std::setw(6) << machine->getCurrent() << " A"
                      << " | RPM: " << std::setw(6) << machine->getRPM()
                      << " | Fan: " << (machine->isFanOn() ? (machine->isFanFailed() ? "FAIL" : "ON ") : "OFF")
                      << " | Pump: " << (machine->isPumpOn() ? (machine->isPumpFailed() ? "FAIL" : "ON ") : "OFF")
                      << " | Flow: " << std::setw(4) << machine->getCoolantFlow() << " L/m"
                      << " | Status: " << statusText << std::endl;

            auto now = std::chrono::system_clock::now();

            if (!machine->isSensorFailed()) {
                healthMonitor.recordHeartbeat(mId, "temperature_sensor");
            }
            healthMonitor.recordHeartbeat(mId, "current_sensor");
            healthMonitor.recordHeartbeat(mId, "rpm_sensor");
            healthMonitor.recordHeartbeat(mId, "load_sensor");
            healthMonitor.recordHeartbeat(mId, "coolant_flow_sensor");
            healthMonitor.recordHeartbeat(mId, "fan_status");
            healthMonitor.recordHeartbeat(mId, "pump_status");
            healthMonitor.recordHeartbeat(mId, "machine_status");

            double reportedTemp = machine->getTemperature();
            std::string tempQuality = "good";
            if (machine->isSensorFailed()) {
                reportedTemp = -999.0;
                tempQuality = "bad";
            }

            Telemetry tTemp{mId, "temperature_sensor", "temperature", reportedTemp, tempQuality, "C", now};
            Telemetry tCur{mId, "current_sensor", "current", machine->getCurrent(), "good", "A", now};
            Telemetry tRpm{mId, "rpm_sensor", "rpm", machine->getRPM(), "good", "RPM", now};
            Telemetry tLoad{mId, "load_sensor", "load", machine->getLoad(), "good", "%", now};
            Telemetry tFlow{mId, "coolant_flow_sensor", "coolant_flow", machine->getCoolantFlow(), "good", "L/min", now};
            Telemetry tFan{mId, "fan_status", "fan", machine->isFanOn() ? 1.0 : 0.0, "good", "state", now};
            Telemetry tPump{mId, "pump_status", "pump", machine->isPumpOn() ? 1.0 : 0.0, "good", "state", now};
            Telemetry tStatus{mId, "machine_status", "status", static_cast<double>(statusValue), "good", "state", now};

            std::vector<Telemetry> machineItems = {tTemp, tCur, tRpm, tLoad, tFlow, tFan, tPump, tStatus};

            for (auto& item : machineItems) {
                telemetryProcessor.process(item);
                cycleBatch.push_back(item);
            }

            if (mqttConnected) {
                std::string prefix = "factory/" + mId + "/telemetry/";
                mqtt.publish(prefix + "temperature", std::to_string(reportedTemp));
                mqtt.publish(prefix + "current", std::to_string(machine->getCurrent()));
                mqtt.publish(prefix + "rpm", std::to_string(machine->getRPM()));
                mqtt.publish(prefix + "load", std::to_string(machine->getLoad()));
                mqtt.publish(prefix + "coolant_flow", std::to_string(machine->getCoolantFlow()));
                mqtt.publish(prefix + "fan", machine->isFanOn() ? "ON" : "OFF");
                mqtt.publish(prefix + "pump", machine->isPumpOn() ? "ON" : "OFF");
                mqtt.publish(prefix + "status", statusText);
                mqtt.publish("factory/" + mId + "/heartbeat", "{\"machine\":\"" + mId + "\",\"timestamp\":" + std::to_string(elapsedSeconds) + ",\"status\":\"" + statusText + "\"}");
            }
        }

        healthMonitor.checkHealth();
        auto offlineDevs = healthMonitor.getOfflineDevices();
        if (!offlineDevs.empty()) {
            std::cout << "[HEALTH-ALERT] " << offlineDevs.size() << " device(s) offline/unresponsive:" << std::endl;
            for (const auto& dev : offlineDevs) {
                std::cout << "  -> " << dev.machineId << " / " << dev.deviceId << std::endl;
            }
        }

        bool influxBatchOk = influx.writeBatch(cycleBatch);
        std::cout << "[INFLUX] Batch telemetry (" << cycleBatch.size() << " points): "
                  << (influxBatchOk ? "OK (Written to InfluxDB)" : "FAILED (InfluxDB offline/unreachable)") << std::endl;

        const auto& allEvents = eventLogger.getEvents();
        if (allEvents.size() > loggedEventsIndex) {
            std::vector<Event> newEvents;
            for (size_t i = loggedEventsIndex; i < allEvents.size(); ++i) {
                newEvents.push_back(allEvents[i]);
                if (mqttConnected) {
                    std::string eventTopic = "factory/" + allEvents[i].machineId + "/events";
                    std::string eventPayload = "{\"severity\":\"" + Event::severityToString(allEvents[i].severity) + "\",\"type\":\"" + allEvents[i].eventType + "\",\"message\":\"" + allEvents[i].message + "\"}";
                    mqtt.publish(eventTopic, eventPayload);
                }
            }
            influx.writeEvents(newEvents);
            loggedEventsIndex = allEvents.size();
        }

        if (!anyActive && elapsedSeconds > 35) {
            std::cout << "\n[SIMULATION] All machines have halted safely. Concluding simulation run." << std::endl;
            break;
        }

        elapsedSeconds++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n======================================================" << std::endl;
    std::cout << "   Simulation Complete" << std::endl;
    std::cout << "   Total Elapsed: " << elapsedSeconds << " seconds" << std::endl;
    std::cout << "   Total Events Recorded: " << eventLogger.getEventCount() << std::endl;
    std::cout << "======================================================" << std::endl;

    if (mqttConnected) {
        mqtt.disconnect();
    }

    return 0;
}