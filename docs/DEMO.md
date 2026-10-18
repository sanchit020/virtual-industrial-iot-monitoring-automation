# Demonstration & Verification Guide

This guide details the programmed demonstration scenario, interactive MQTT control protocols (including broadcast routing), safety recovery rules, and the automated verification suite.

---

## 1. Programmed 50-Second Multi-Machine Timeline

The platform executable (`virtual-industrial-iot.exe`) contains a 50-second multi-machine demonstration loop implemented in `src/main.cpp`. Each cycle executes at a deterministic 1-second interval, advancing simulation physics, evaluating automation rules, and logging telemetry:

| Time (s) | Target Machine | Injected Fault / Scenario | Implementation & System Reaction |
|---|---|---|---|
| **0s** | All Machines | **Platform Initialization** | Registers `machine1` (Machine A), `machine2` (Machine B), and `machine3` (Machine C). Health monitor registers sensors and actuators. Baseline: 25°C, 60% load, 600 A, 1800 RPM. |
| **15s** | Machine B (`machine2`) | **Fan Mechanical Failure** | `mb->triggerFanFailure()`. Actuator reports state `2` (FAILED) with quality `"bad"`. When temperature exceeds 70°C, forced-air convection is unavailable, prompting pump engagement at 80°C. |
| **20s** | Machine A (`machine1`) | **Electrical Overload (200% Load)** | `ma->triggerOverload(200.0)`. Electrical current reaches 2000 A, motor speed reaches 6000 RPM. Rapid heating (+4.0°C/s) activates Fan at 70°C and Pump at 80°C. |
| **25s** | Machine C (`machine3`) | **Sensor Failure (Sentinel: `-999.0°C`)** | `mc->triggerSensorFailure()`. Sensor begins reporting `-999.0°C`. `TelemetryProcessor` marks quality as `"invalid"`. Automation enters fail-safe cooling mode to protect unmonitored equipment. |
| **30s** | Machine C (`machine3`) | **Coolant Pump Motor Failure + 150% Load** | `mc->triggerPumpFailure()` with load set to 150%. Liquid coolant flow zeroes (`Flow: 0.0 L/m`). Pump reports state `2` (FAILED). Machine undergoes thermal escalation. |
| **35s+** | Machine B & C | **Device Health Alerts** | `DeviceHealthMonitor` detects hardware failure on Machine B fan and Machine C pump, and invalid data on Machine C temperature sensor: `[HEALTH-ALERT] 3 device(s) in fault/degraded condition`. |
| **51s** | All Machines | **Simulation Complete** | Clean shutdown, final metrics reported, 20 clean edge-triggered events stored in InfluxDB, and MQTT disconnected. |

> **Note on Sentinel Sensor Value (`-999.0°C`)**:
> The `-999.0°C` reading is a **custom simulated sensor-failure marker** representing a hardware failure (such as an open thermocouple or corrupted ADC read). The telemetry validation pipeline marks these records with `quality = "invalid"`, and Grafana filters out sub-zero readings (`r._value >= 0.0 and r.quality == "good"`) so that temperature charts are not distorted while the event log retains the fault history.

---

## 2. Interactive MQTT Command & Control Protocol

`src/main.cpp` subscribes to both point-to-point topics (`factory/<machineId>/command`) and the broadcast topic (`factory/all/command`). Commands are routed through a thread-safe `CommandQueue` and executed deterministically on the simulation thread.

### Point-to-Point Machine Commands

#### 1. Emergency Machine Shutdown
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine2/command" -m "STOP"
```
**JSON Acknowledgement on `factory/machine2/command/ack`**:
```json
{"machine":"machine2","command":"STOP","status":"SUCCESS","message":"Emergency shutdown executed"}
```

#### 2. Electrical Load Setpoint Adjustment
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine1/command" -m "SET_LOAD 85.0"
```
**JSON Acknowledgement on `factory/machine1/command/ack`**:
```json
{"machine":"machine1","command":"SET_LOAD","status":"SUCCESS","message":"Load set to 85.000000%"}
```

#### 3. Manual Actuator Override
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine1/command" -m "SET_FAN ON"
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine1/command" -m "SET_PUMP ON"
```

#### 4. Fault Injection
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine3/command" -m "INJECT_FAULT PUMP"
```
Supported fault types: `FAN`, `PUMP`, `SENSOR`, `OVERLOAD`.

#### 5. Safe Fault Reset & Recovery
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine3/command" -m "RESET_FAULT"
```
* **If Machine is Hot ($\ge 80^\circ\text{C}$)**:
  ```json
  {"machine":"machine3","command":"RESET_FAULT","status":"FAILED","message":"Recovery rejected: Core temperature 85 C is above safe threshold (80 C). Wait for cooldown."}
  ```
* **If Machine has Cooled ($< 80^\circ\text{C}$)**:
  ```json
  {"machine":"machine3","command":"RESET_FAULT","status":"SUCCESS","message":"Machine recovered to normal operational state"}
  ```

---

### Broadcast Machine Commands (`factory/all/command`)

The `factory/all/command` topic dispatches commands simultaneously to all machines in the factory:

#### Broadcast Factory-Wide Emergency Stop
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/all/command" -m "STOP"
```
**Aggregated JSON Acknowledgement on `factory/all/command/ack`**:
```json
{
  "target": "all",
  "command": "STOP",
  "status": "SUCCESS",
  "executed": 3,
  "total": 3,
  "details": [
    {"machine": "machine1", "status": "SUCCESS", "message": "Emergency shutdown executed"},
    {"machine": "machine2", "status": "SUCCESS", "message": "Emergency shutdown executed"},
    {"machine": "machine3", "status": "SUCCESS", "message": "Emergency shutdown executed"}
  ]
}
```

#### Broadcast Load Normalization
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/all/command" -m "SET_LOAD 60.0"
```

#### Broadcast Safe Recovery
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/all/command" -m "RESET_FAULT"
```

---

## 3. Automated Verification Test Suite

Run the automated test suite to verify all core functional modules:
```powershell
.\build\Release\industrial-iot-tests.exe
```
Output:
```text
======================================================
   Virtual Industrial IoT Automated Test Suite
   (Release-mode active assertion validation)
======================================================
[PASS] testAssertionIntegrity (Release-mode active verification)
[PASS] testTelemetryValidator
[PASS] testAutomationEngineRules
[PASS] testMachinePhysicsAndFaults
[PASS] testMachineRecoveryRules (Safe cooldown verification)
[PASS] testDeviceHealthMonitor (Heartbeat vs validity vs operational state)
[PASS] testConfigManager (JSON parsing, clearing on reload, threshold validation)
[PASS] testMQTTCommandLogic (Command execution, actuator separation, and shutdown telemetry)

ALL 8 AUTOMATED TEST SUITES PASSED SUCCESSFULLY (100% VALIDATED)!
```
