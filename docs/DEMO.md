# Demonstration & Verification Guide

This guide details the end-to-end demonstration scenario, interactive MQTT control, and verification suite built into the Virtual Industrial IoT Platform.

---

## 1. Programmed 50-Second Demonstration Timeline

The platform executable (`virtual-industrial-iot.exe`) contains a programmed 50-second multi-machine demonstration loop implemented directly in `src/main.cpp` (`while (elapsedSeconds <= maxSimulationSeconds)`). Each cycle executes at a deterministic 1-second interval, advancing simulation physics, evaluating automation rules, and logging telemetry:

| Time (s) | Target Machine | Injected Fault / Operational Event | Implementation & Expected System Reaction |
|---|---|---|---|
| **0s** | All Machines | **Factory Startup** | All 3 machines register (`Machine A`, `Machine B`, `Machine C`), devices online, initial heartbeats logged. Baseline: ~26°C, 60% load, 1800 RPM, 600 A. |
| **15s** | Machine B (`machine2`) | **Fan Mechanical Failure** | `mb->triggerFanFailure()`. Actuator state reports `Fan: FAIL`. When temperature exceeds 70°C, fan forced-air cooling is unavailable, and Machine B transitions to `WARNING`. |
| **20s** | Machine A (`machine1`) | **Electrical Overload (200% Load)** | `ma->triggerOverload(200.0)`. Electrical current jumps to 2000 A, motor speed to 6000 RPM. Rapid heating (+4.0°C/s) triggers Fan at 70°C and Pump at 80°C. |
| **25s** | Machine C (`machine3`) | **Sensor Failure (Sentinel: `-999.0°C`)** | `mc->triggerSensorFailure()`. Sensor begins reporting `-999.0°C` (the simulation's custom sentinel error value). `TelemetryValidator` marks quality as `invalid`. |
| **30s** | Machine C (`machine3`) | **Coolant Pump Failure + 150% Load** | `mc->triggerPumpFailure()` with load set to 150%. Liquid coolant flow remains `0.0 L/min`. Machine C undergoes thermal escalation without cooling. |
| **35s** | Machine C (`machine3`) | **Device Health Alert** | `DeviceHealthMonitor` detects missing temperature heartbeats exceeding the 10-second threshold and raises `[HEALTH-ALERT] machine3 / temperature_sensor`. |
| **45s** | Machine C (`machine3`) | **Emergency Fail-Safe Shutdown** | Upon reaching 96°C ($\ge 95^\circ\text{C}$ threshold), deterministic safety logic trips: power/load cuts to 0%, current drops to 0 A, and machine enters `SHUTDOWN`. |
| **51s** | All Machines | **Simulation Complete** | Clean shutdown, final metrics reported, 42 structured events recorded, and MQTT disconnected. |

> **Note on Sentinel Sensor Value (`-999.0°C`)**:
> The `-999.0°C` reading is the **simulation's custom out-of-band sentinel value**, designed to distinguish a broken sensor probe from legitimate zero or ambient temperature readings. This allows the `TelemetryValidator` and `DeviceHealthMonitor` to flag corrupted hardware channels and trigger maintenance alerts.

---

## 2. Interactive MQTT Command & Control Protocol

In addition to the autonomous timeline, `src/main.cpp` subscribes to `factory/<machineId>/command` and `factory/all/command`. The platform parses incoming text commands, applies the required state modification to the virtual machine, logs an event, and publishes a structured JSON acknowledgement to `factory/<machineId>/command/ack`.

While the simulation is running, open a separate terminal to send real-time commands and observe live execution:

### 1. Emergency Machine Shutdown
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine2/command" -m "STOP"
```
**JSON Acknowledgement on `factory/machine2/command/ack`**:
```json
{"machine":"machine2","command":"STOP","status":"SUCCESS","message":"Shutdown command executed"}
```

### 2. Manual Electrical Load Setpoint
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine1/command" -m "SET_LOAD 75.0"
```
**JSON Acknowledgement on `factory/machine1/command/ack`**:
```json
{"machine":"machine1","command":"SET_LOAD","status":"SUCCESS","message":"Load set to 75.000000%"}
```

### 3. Cooling Actuator Manual Override
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine1/command" -m "SET_FAN ON"
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine1/command" -m "SET_PUMP ON"
```
**JSON Acknowledgement on `factory/machine1/command/ack`**:
```json
{"machine":"machine1","command":"SET_FAN","status":"SUCCESS","message":"Fan turned ON"}
```

### 4. Dynamic Fault Injection
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine3/command" -m "INJECT_FAULT PUMP"
```
Supported fault types: `FAN`, `PUMP`, `SENSOR`, `OVERLOAD`.
**JSON Acknowledgement on `factory/machine3/command/ack`**:
```json
{"machine":"machine3","command":"INJECT_FAULT","status":"SUCCESS","message":"Pump failure injected"}
```

### 5. Fault Reset & Recovery
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine3/command" -m "RESET_FAULT"
```
**JSON Acknowledgement on `factory/machine3/command/ack`**:
```json
{"machine":"machine3","command":"RESET_FAULT","status":"SUCCESS","message":"Faults cleared"}
```

---

## 3. Automated Verification Test Suite

Run the automated test binary to verify rules, validators, physics, health monitoring, configuration, and command logic:
```powershell
.\build\Release\industrial-iot-tests.exe
```
Expected output:
```text
Running Industrial IoT Automated Test Suite...
[PASS] TelemetryValidator tests passed
[PASS] AutomationEngine rules tests passed
[PASS] Machine physics and faults tests passed
[PASS] DeviceHealthMonitor tests passed
[PASS] ConfigManager tests passed
[PASS] MQTT command processing logic tests passed

ALL AUTOMATED TESTS PASSED SUCCESSFULLY!
```
