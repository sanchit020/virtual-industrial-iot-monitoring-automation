#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <queue>

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

struct QueuedCommand {
    std::string topic;
    std::string message;
};

class CommandQueue {
private:
    std::mutex mtx;
    std::queue<QueuedCommand> queue;

public:
    void push(const std::string& topic, const std::string& message) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push({topic, message});
    }

    bool pop(QueuedCommand& cmd) {
        std::lock_guard<std::mutex> lock(mtx);
        if (queue.empty()) {
            return false;
        }
        cmd = queue.front();
        queue.pop();
        return true;
    }
};

std::string getEnvOrDefault(const char* varName, const std::string& defaultValue) {
    const char* val = std::getenv(varName);
    if (val != nullptr && std::string(val).length() > 0) {
        return std::string(val);
    }
    return defaultValue;
}

void executeSingleMachineCommand(Machine* machine,
                                 const std::string& cmd,
                                 const std::string& valStr,
                                 bool& success,
                                 std::string& responseMsg,
                                 EventLogger& logger) {
    const std::string& machineId = machine->getId();

    if (cmd == "STOP" || cmd == "SHUTDOWN") {
        machine->triggerShutdown();
        success = true;
        responseMsg = "Emergency shutdown executed";
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
                success = false;
                responseMsg = "Load parameter out of range (0-300%)";
            }
        } catch (...) {
            success = false;
            responseMsg = "Invalid load numeric parameter";
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
            success = false;
            responseMsg = "Invalid fan state parameter (use ON/OFF)";
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
            success = false;
            responseMsg = "Invalid pump state parameter (use ON/OFF)";
        }
        if (success) {
            logger.log(machineId, "COMMAND_EXECUTED", responseMsg, EventSeverity::INFO);
        }
    } else if (cmd == "RESET_FAULT" || cmd == "CLEAR_FAULTS" || cmd == "RECOVER") {
        std::string rejectionReason;
        if (machine->attemptRecovery(rejectionReason)) {
            success = true;
            responseMsg = rejectionReason;
            logger.log(machineId, "COMMAND_EXECUTED", responseMsg, EventSeverity::INFO);
        } else {
            success = false;
            responseMsg = rejectionReason;
            logger.log(machineId, "COMMAND_REJECTED", responseMsg, EventSeverity::WARNING);
        }
    } else if (cmd == "INJECT_FAULT") {
        if (valStr == "FAN") {
            machine->triggerFanFailure();
            success = true;
            responseMsg = "Fan mechanical failure injected";
        } else if (valStr == "PUMP") {
            machine->triggerPumpFailure();
            success = true;
            responseMsg = "Pump motor failure injected";
        } else if (valStr == "SENSOR") {
            machine->triggerSensorFailure();
            success = true;
            responseMsg = "Sensor failure injected (-999 C sentinel)";
        } else if (valStr == "OVERLOAD") {
            machine->triggerOverload(200.0);
            success = true;
            responseMsg = "Electrical overload injected (200% load)";
        } else {
            success = false;
            responseMsg = "Unknown fault injection parameter: " + valStr;
        }
        if (success) {
            logger.log(machineId, "FAULT_INJECTED", responseMsg, EventSeverity::WARNING);
        }
    } else {
        success = false;
        responseMsg = "Unrecognized command: " + cmd;
    }
}

void processCommand(const std::string& topic,
                    const std::string& message,
                    Factory& factory,
                    MQTTClient& mqtt,
                    EventLogger& logger) {
    std::cout << "[MQTT-CMD] Processing from queue [" << topic << "]: " << message << std::endl;

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

    std::string cmd = message;
    std::string valStr = "";
    size_t spacePos = message.find(' ');
    if (spacePos != std::string::npos) {
        cmd = message.substr(0, spacePos);
        valStr = message.substr(spacePos + 1);
    }

    if (machineId == "all") {
        std::string ackTopic = "factory/all/command/ack";
        int totalSuccess = 0;
        int totalAttempted = 0;
        std::ostringstream details;

        for (const auto& mPtr : factory.getMachines()) {
            Machine* m = mPtr.get();
            bool s = false;
            std::string rMsg;
            executeSingleMachineCommand(m, cmd, valStr, s, rMsg, logger);
            totalAttempted++;
            if (s) {
                totalSuccess++;
            }
            details << "{\\\"machine\\\":\\\"" << m->getId() << "\\\",\\\"status\\\":\\\""
                    << (s ? "SUCCESS" : "FAILED") << "\\\",\\\"message\\\":\\\"" << rMsg << "\\\"},";
        }

        std::string dStr = details.str();
        if (!dStr.empty() && dStr.back() == ',') {
            dStr.pop_back();
        }

        std::string ackPayload = "{\"target\":\"all\",\"command\":\"" + cmd +
                                 "\",\"status\":\"" + (totalSuccess == totalAttempted ? "SUCCESS" : "PARTIAL") +
                                 "\",\"executed\":" + std::to_string(totalSuccess) +
                                 ",\"total\":" + std::to_string(totalAttempted) +
                                 ",\"details\":[" + dStr + "]}";
        mqtt.publish(ackTopic, ackPayload);
        std::cout << "[MQTT-ACK] " << ackTopic << " -> " << ackPayload << std::endl;
        return;
    }

    Machine* machine = factory.getMachine(machineId);
    std::string ackTopic = "factory/" + machineId + "/command/ack";

    if (!machine) {
        std::string nack = "{\"machine\":\"" + machineId + "\",\"status\":\"ERROR\",\"message\":\"Machine not found\"}";
        mqtt.publish(ackTopic, nack);
        logger.log(machineId, "COMMAND_REJECTED", "Target machine not found: " + machineId, EventSeverity::WARNING);
        return;
    }

    bool success = false;
    std::string responseMsg;
    executeSingleMachineCommand(machine, cmd, valStr, success, responseMsg, logger);

    std::string ackPayload = "{\"machine\":\"" + machineId + "\",\"command\":\"" + cmd +
                             "\",\"status\":\"" + (success ? "SUCCESS" : "FAILED") +
                             "\",\"message\":\"" + responseMsg + "\"}";
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

    AutomationEngine automation(config.fanThreshold, config.pumpThreshold, config.shutdownThreshold);
    EventLogger eventLogger;
    TelemetryProcessor telemetryProcessor;
    CommandQueue commandQueue;

    MQTTClient mqtt(config.mqttBroker, config.mqttPort);
    bool mqttConnected = mqtt.connect();
    if (!mqttConnected) {
        std::cerr << "[MQTT-WARN] Could not connect to MQTT broker at "
                  << config.mqttBroker << ":" << config.mqttPort
                  << ". Running simulation without active MQTT network." << std::endl;
    } else {
        for (const auto& m : config.machines) {
            std::string cmdTopic = "factory/" + m.id + "/command";
            mqtt.subscribe(cmdTopic, [&commandQueue](const std::string& t, const std::string& msg) {
                commandQueue.push(t, msg);
            });
        }
        mqtt.subscribe("factory/all/command", [&commandQueue](const std::string& t, const std::string& msg) {
            commandQueue.push(t, msg);
        });
    }

    std::string influxUrl = getEnvOrDefault("INFLUX_URL", config.influxUrl);
    std::string influxOrg = getEnvOrDefault("INFLUX_ORG", config.influxOrg);
    std::string influxBucket = getEnvOrDefault("INFLUX_BUCKET", config.influxBucket);
    std::string influxToken = getEnvOrDefault("INFLUX_TOKEN", config.influxToken);

    std::cout << "[INFLUX] Target URL: " << influxUrl
              << " | Org: " << influxOrg
              << " | Bucket: " << influxBucket << std::endl;

    if (influxToken.empty()) {
        std::cerr << "[INFLUX-WARN] No InfluxDB token detected. Set the INFLUX_TOKEN environment variable." << std::endl;
    }

    InfluxWriter influx(influxUrl, influxOrg, influxBucket, influxToken);

    eventLogger.log("factory", "SYSTEM_START", "Virtual Industrial IoT Platform initialized", EventSeverity::INFO);

    int elapsedSeconds = 0;
    const int maxSimulationSeconds = 50;
    size_t loggedEventsIndex = 0;

    while (elapsedSeconds <= maxSimulationSeconds) {
        QueuedCommand qCmd;
        while (commandQueue.pop(qCmd)) {
            processCommand(qCmd.topic, qCmd.message, factory, mqtt, eventLogger);
        }

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
                eventLogger.log("machine3", "FAULT_INJECTED", "Simulated temperature sensor failure (-999 C sentinel)", EventSeverity::WARNING);
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
            } else if (machine->getTemperature() >= config.pumpThreshold || machine->hasFault()) {
                statusValue = 1;
                statusText = "WARNING";
            } else {
                statusValue = 0;
                statusText = "RUNNING";
            }

            std::string fanText = machine->isFanFailed() ? "FAIL" : (machine->isFanRunning() ? "ON " : "OFF");
            std::string pumpText = machine->isPumpFailed() ? "FAIL" : (machine->isPumpRunning() ? "ON " : "OFF");

            std::cout << std::left << std::setw(12) << mName
                      << " | Temp: " << std::fixed << std::setprecision(1) << std::setw(5) << machine->getTemperature() << " C"
                      << " | Load: " << std::setw(5) << machine->getLoad() << " %"
                      << " | Cur: " << std::setw(6) << machine->getCurrent() << " A"
                      << " | RPM: " << std::setw(6) << machine->getRPM()
                      << " | Fan: " << fanText
                      << " | Pump: " << pumpText
                      << " | Flow: " << std::setw(4) << machine->getCoolantFlow() << " L/m"
                      << " | Status: " << statusText << std::endl;

            auto now = std::chrono::system_clock::now();

            bool sensorValid = !machine->isSensorFailed();
            healthMonitor.updateDeviceStatus(mId, "temperature_sensor", true, sensorValid, true,
                sensorValid ? "Sensor operational" : "Sensor reading invalid (-999 C sentinel)");
            healthMonitor.updateDeviceStatus(mId, "current_sensor", true, true, true, "Sensor operational");
            healthMonitor.updateDeviceStatus(mId, "rpm_sensor", true, true, true, "Sensor operational");
            healthMonitor.updateDeviceStatus(mId, "load_sensor", true, true, true, "Sensor operational");
            healthMonitor.updateDeviceStatus(mId, "coolant_flow_sensor", true, true, true, "Sensor operational");
            healthMonitor.updateDeviceStatus(mId, "fan_status", true, true, !machine->isFanFailed(),
                machine->isFanFailed() ? "Fan mechanical failure" : (machine->isFanRunning() ? "Active cooling" : "Standby"));
            healthMonitor.updateDeviceStatus(mId, "pump_status", true, true, !machine->isPumpFailed(),
                machine->isPumpFailed() ? "Pump motor failure" : (machine->isPumpRunning() ? "Active liquid cooling" : "Standby"));
            healthMonitor.updateDeviceStatus(mId, "machine_status", true, true, !machine->isShutdown(),
                machine->isShutdown() ? "Shutdown" : "Operational");

            double reportedTemp = machine->getTemperature();
            std::string tempQuality = "good";
            if (machine->isSensorFailed()) {
                reportedTemp = -999.0;
                tempQuality = "bad";
            }

            double fanTelemState = static_cast<double>(machine->getFanTelemetryState());
            double pumpTelemState = static_cast<double>(machine->getPumpTelemetryState());
            std::string fanQuality = machine->isFanFailed() ? "bad" : "good";
            std::string pumpQuality = machine->isPumpFailed() ? "bad" : "good";

            Telemetry tTemp{mId, "temperature_sensor", "temperature", reportedTemp, tempQuality, "C", now};
            Telemetry tCur{mId, "current_sensor", "current", machine->getCurrent(), "good", "A", now};
            Telemetry tRpm{mId, "rpm_sensor", "rpm", machine->getRPM(), "good", "RPM", now};
            Telemetry tLoad{mId, "load_sensor", "load", machine->getLoad(), "good", "%", now};
            Telemetry tFlow{mId, "coolant_flow_sensor", "coolant_flow", machine->getCoolantFlow(), "good", "L/min", now};
            Telemetry tFan{mId, "fan_status", "fan", fanTelemState, fanQuality, "state", now};
            Telemetry tPump{mId, "pump_status", "pump", pumpTelemState, pumpQuality, "state", now};
            Telemetry tStatus{mId, "machine_status", "status", static_cast<double>(statusValue), "good", "state", now};

            std::vector<Telemetry> machineItems = {tTemp, tCur, tRpm, tLoad, tFlow, tFan, tPump, tStatus};

            for (auto& item : machineItems) {
                bool processed = telemetryProcessor.process(item);
                if (!processed && item.sensorType == "temperature") {
                    std::cout << "[TELEMETRY-VALIDATOR] Tagged invalid reading on " << mId << " (" << item.value << " C)" << std::endl;
                }
                cycleBatch.push_back(item);
            }

            if (mqttConnected) {
                std::string prefix = "factory/" + mId + "/telemetry/";
                mqtt.publish(prefix + "temperature", std::to_string(reportedTemp));
                mqtt.publish(prefix + "current", std::to_string(machine->getCurrent()));
                mqtt.publish(prefix + "rpm", std::to_string(machine->getRPM()));
                mqtt.publish(prefix + "load", std::to_string(machine->getLoad()));
                mqtt.publish(prefix + "coolant_flow", std::to_string(machine->getCoolantFlow()));
                mqtt.publish(prefix + "fan", machine->isFanFailed() ? "FAILED" : (machine->isFanRunning() ? "ON" : "OFF"));
                mqtt.publish(prefix + "pump", machine->isPumpFailed() ? "FAILED" : (machine->isPumpRunning() ? "ON" : "OFF"));
                mqtt.publish(prefix + "fan_state", std::to_string(machine->getFanTelemetryState()));
                mqtt.publish(prefix + "pump_state", std::to_string(machine->getPumpTelemetryState()));
                mqtt.publish(prefix + "status", statusText);
                mqtt.publish("factory/" + mId + "/heartbeat", "{\"machine\":\"" + mId + "\",\"timestamp\":" + std::to_string(elapsedSeconds) + ",\"status\":\"" + statusText + "\"}");
            }
        }

        healthMonitor.checkHealth();
        auto unhealthyDevs = healthMonitor.getUnhealthyDevices();
        if (!unhealthyDevs.empty()) {
            std::cout << "[HEALTH-ALERT] " << unhealthyDevs.size() << " device(s) in fault/degraded condition:" << std::endl;
            for (const auto& dev : unhealthyDevs) {
                std::cout << "  -> " << dev.machineId << " / " << dev.deviceId << ": " << dev.statusSummary
                          << " (" << dev.detail << ")" << std::endl;
            }
        }

        bool influxBatchOk = influx.writeBatch(cycleBatch);
        std::cout << "[INFLUX] Batch telemetry (" << cycleBatch.size() << " points): "
                  << (influxBatchOk ? "OK (Stored)" : "BUFFERED (Offline or retry pending: " + std::to_string(influx.getPendingTelemetryCount()) + " points)") << std::endl;

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
            bool eventsOk = influx.writeEvents(newEvents);
            std::cout << "[INFLUX] Events (" << newEvents.size() << " new): "
                      << (eventsOk ? "OK (Stored)" : "BUFFERED (Retry pending: " + std::to_string(influx.getPendingEventCount()) + " events)") << std::endl;
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