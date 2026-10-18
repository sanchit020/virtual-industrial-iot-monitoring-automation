# Demonstration & Verification Guide

This guide details the end-to-end demonstration scenario built into the Virtual Industrial IoT Platform.

## Demonstration Scenario Timeline

When launched, the simulation runs a synchronized multi-machine industrial scenario over 50 seconds:

| Time (s) | Target Machine | Event / Fault Trigger | Expected System Reaction |
|---|---|---|---|
| **0s** | All Machines | Factory Startup | All 3 machines register (`Machine A`, `Machine B`, `Machine C`), devices online, heartbeats recorded. Baseline: 25 C, 60% load, 1800 RPM, 600 A. |
| **15s** | Machine B (`machine2`) | **Fan Mechanical Failure** | Automation attempts to cool when reaching 70 C, but fan cooling fails. Machine B enters WARNING state, logging critical fault event. |
| **20s** | Machine A (`machine1`) | **Electrical Overload (200% Load)** | Machine A load increases from 60% to 200%. Current jumps to 2000 A. Rapid heating triggers Fan at 70 C, Pump at 80 C, and Emergency Shutdown at 95 C. |
| **25s** | Machine C (`machine3`) | **Sensor Failure** | Temperature sensor on Machine C begins reporting `-999 C` with `bad` quality. DeviceHealthMonitor reports missing heartbeat. |
| **30s** | Machine C (`machine3`) | **Coolant Pump Motor Failure** | Machine C pump seizes while load is set to 150%. Without liquid cooling, thermal escalation occurs. |
| **35-45s** | Affected Machines | **Emergency Safe Halts** | Machines reaching 95 C trigger deterministic shutdowns, cutting electrical load to 0% and initiating passive cooldown. |

---

## Interactive MQTT Command Testing

While the simulation is running, open a separate terminal to send real-time commands and observe acknowledgements:

### 1. Emergency Machine Shutdown
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine2/command" -m "STOP"
```
**Expected Response on `factory/machine2/command/ack`**:
```json
{"machine":"machine2","command":"STOP","status":"SUCCESS","message":"Shutdown command executed"}
```

### 2. Manual Setpoint Adjustment
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine1/command" -m "SET_LOAD 75.0"
```
**Expected Response on `factory/machine1/command/ack`**:
```json
{"machine":"machine1","command":"SET_LOAD","status":"SUCCESS","message":"Load set to 75.000000%"}
```

### 3. Fault Injection Command
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine3/command" -m "INJECT_FAULT PUMP"
```
**Expected Response on `factory/machine3/command/ack`**:
```json
{"machine":"machine3","command":"INJECT_FAULT","status":"SUCCESS","message":"Pump failure injected"}
```

### 4. Fault Reset Command
```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "factory/machine2/command" -m "RESET_FAULT"
```
**Expected Response on `factory/machine2/command/ack`**:
```json
{"machine":"machine2","command":"RESET_FAULT","status":"SUCCESS","message":"Faults cleared"}
```

---

## Automated Verification

Run the unit test suite to verify all rules, validators, physics formulas, and device monitors:
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

ALL AUTOMATED TESTS PASSED SUCCESSFULLY!
```
