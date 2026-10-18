# Virtual Industrial IoT Platform

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C.svg)](https://cmake.org/)
[![MQTT](https://img.shields.io/badge/MQTT-Mosquitto-3C5280.svg)](https://mosquitto.org/)
[![InfluxDB](https://img.shields.io/badge/InfluxDB-v2-22ADF6.svg)](https://www.influxdata.com/)
[![Grafana](https://img.shields.io/badge/Grafana-Unified_Dashboard-F46800.svg)](https://grafana.com/)
[![Tests](https://img.shields.io/badge/Unit_Tests-100%25_Passing-brightgreen.svg)]()
[![Platform](https://img.shields.io/badge/Platform-Windows_x64-0078D6.svg)]()

A production-grade, C++17 software simulation of an industrial manufacturing plant. The platform simulates three virtual industrial machines (**Machine A**, **Machine B**, and **Machine C**) equipped with virtual sensors and cooling actuators, implementing deterministic rule-based industrial automation, fault injection scenarios, device health monitoring, real-time MQTT messaging, InfluxDB time-series storage, and a unified Grafana SCADA dashboard.

---

## Grafana Unified Factory Dashboard

The platform streams real-time telemetry into InfluxDB v2, visualized live through an executive-grade 11-panel Grafana dashboard.

### 1. Live Machine Operating Health
Continuous assessment of all machines across the plant floor, displaying operational status based on temperature thresholds and hardware health.
![Factory Machine Operating Status](dashboards/factory_operating_status.png)

### 2. Thermal Control & Automated Closed-Loop Cooling
Real-time temperature curves plotted directly against safety limits (**70°C Fan**, **80°C Pump**, **95°C Emergency Shutdown**), alongside discrete state timelines for cooling actuators.
![Temperature & Automated Closed-Loop Cooling](dashboards/temperature_automated_cooling.png)

### 3. Operating Telemetry: Electrical Load, Current, RPM & Coolant Flow
Live gauges and multi-line time series tracking electrical load setpoints (spiking to 200% under simulated overload), phase current (A), motor speed (RPM), and coolant flow rate (L/min).
![Operating Telemetry](dashboards/operating_telemetry.png)

### 4. Industrial Fault & Alarm Event Log (SCADA Audit Trail)
Chronological record of platform events, fault injections, safety trips, and actuator state transitions with color-coded severity badges (`CRITICAL`, `WARNING`, `INFO`).
![Industrial Fault & Alarm Event Log](dashboards/scada_event_log.png)

---

## System Architecture

```
                      +-----------------------------+
                      |   Virtual Factory Engine    |
                      | (Machine A, B, C Physics)   |
                      +--------------+--------------+
                                     |
                +--------------------+--------------------+
                |                                         |
                v                                         v
   +------------------------+                +-------------------------+
   |   Automation Engine    |                |  Device Health Monitor  |
   | (70°C Fan, 80°C Pump,  |                | (Heartbeats, Timeouts,  |
   |  95°C Emergency Stop)  |                |  Missing Sensor Alarms) |
   +------------+-----------+                +------------+------------+
                |                                         |
                +--------------------+--------------------+
                                     |
                                     v
                        +-------------------------+
                        |   Telemetry Processor   |
                        |  & Validation Pipeline  |
                        +------------+------------+
                                     |
                     +---------------+---------------+
                     |                               |
                     v                               v
       +---------------------------+   +---------------------------+
       |   MQTT Broker (Mosquitto) |   |    InfluxDB v2 Storage    |
       |  Telemetry Topics, Heart- |   |  Line Protocol Batch      |
       |  beat, Commands & ACKs    |   |  Telemetry & Event Store  |
       +-------------+-------------+   +-------------+-------------+
                     |                               |
                     +---------------+---------------+
                                     |
                                     v
                       +---------------------------+
                       |     Grafana Dashboard     |
                       | Status, Gauges, Flow,     |
                       | Actuators & Event History |
                       +---------------------------+
```

---

## Core Platform Capabilities

### 1. Multi-Machine Virtual Factory
Three independent virtual production units simulate thermodynamic and electromechanical behaviors:
* **Machine A** (`machine1`)
* **Machine B** (`machine2`)
* **Machine C** (`machine3`)

#### Dynamic Physics Engine:
* **Thermal Accumulation**: Heat generated proportional to electrical load: `+0.02 * load * dt`
* **Active Air Cooling**: When fan is running: `-1.0 °C/s`
* **Active Liquid Cooling**: When coolant pump is active: `-2.5 °C/s`
* **Electrical Load to Current**: `Current = Load * 10.0 A` (e.g. 60% load = 600 A; 200% overload = 2000 A)
* **Operational Spindle Speed**: `RPM = Load * 30.0 RPM` (e.g. 60% load = 1800 RPM; 200% overload = 6000 RPM)
* **Coolant Flow Rate**: `10.0 L/min` when pump is operational; `0.0 L/min` when pump is OFF or failed

---

### 2. Standardized Telemetry Signals
Every machine emits 8 standardized telemetry signals on every 1-second simulation cycle:

| Signal | Measurement / Sensor | Engineering Unit | Valid Normal Range | Description |
| :--- | :--- | :--- | :--- | :--- |
| **`temperature`** | `temperature_sensor` | `°C` | $-40.0$ to $150.0$ | Core machine operational temperature |
| **`current`** | `current_sensor` | `A` | $0.0$ to $3000.0$ | Three-phase electrical draw |
| **`rpm`** | `rpm_sensor` | `RPM` | $0.0$ to $10000.0$ | Spindle / motor rotational velocity |
| **`load`** | `load_sensor` | `%` | $0.0$ to $300.0$ | Electrical operating setpoint load |
| **`coolant_flow`** | `coolant_flow_sensor` | `L/min` | $0.0$ to $50.0$ | Coolant circulation throughput |
| **`fan`** | `fan_status` | `state` | $0$ (OFF) or $1$ (ON) | Air cooling actuator operational state |
| **`pump`** | `pump_status` | `state` | $0$ (OFF) or $1$ (ON) | Liquid cooling actuator operational state |
| **`status`** | `machine_status` | `state` | $0$ (RUN), $1$ (WARN), $2$ (SHUTDOWN) | Overall machine health classification |

---

### 3. Deterministic Industrial Automation Rules
The rule-based automation engine monitors physical telemetry and enforces deterministic safety boundaries:

```
                  Temperature Progression (°C)
  Ambient (25°C)      70°C              80°C              95°C
  ───────┬─────────────┼─────────────────┼─────────────────┼──────────>
         │  Normal     │   Warning       │   High Temp     │ Emergency
         │  Operation  │   Air Cooling   │   Dual Cooling  │ Shutdown
         │ (Actuators  │  (Fan Auto-ON)  │  (Pump Auto-ON) │ (Power Cut,
         │    OFF)     │                 │   (Flow 10 L/m) │  Load -> 0)
```

1. **Normal Operation ($< 70^\circ\text{C}$)**: Actuators remain OFF to conserve energy. Status: `RUNNING`.
2. **Air Cooling Trigger ($\ge 70^\circ\text{C}$)**: Fan turns ON automatically. Status: `WARNING`.
3. **Liquid Cooling Trigger ($\ge 80^\circ\text{C}$)**: Coolant pump engages (`Flow: 10.0 L/m`), running concurrently with the fan. Status: `WARNING`.
4. **Emergency Fail-Safe Shutdown ($\ge 95^\circ\text{C}$)**: Critical thermal runaway trip. Power and load are immediately cut to 0, actuators deactivated, and status set to `SHUTDOWN`. Machine enters safe ambient cooldown.

---

### 4. Fault Simulation & Diagnostic Signaling
The platform models realistic industrial equipment failure modes:

* **Electrical Overload (Machine A & C)**: Simulated electrical surge to 200% / 150% load, driving rapid thermal runaway ($+4.0^\circ\text{C/s}$).
* **Mechanical Fan Failure (Machine B)**: Fan motor seizes, disabling forced-air convection and demonstrating thermostatic oscillation.
* **Coolant Pump Failure (Machine C)**: Pump motor failure zeroes coolant flow (`Flow: 0.0 L/m`), leaving high-load equipment without liquid cooling and provoking emergency shutdown at 95°C.
* **Sensor Hardware Failure (Machine C)**: Simulates physical sensor failure (e.g. open-circuit thermocouple probe). In accordance with industrial instrumentation standards (NAMUR NE 43):
  * Emits an out-of-range sentinel error code: **`-999.0°C`**.
  * Marks telemetry quality as `"bad"` / `"invalid"`.
  * Suspends sensor heartbeats, prompting automated health detection.

---

### 5. Device Availability & Missing-Heartbeat Detection
The `DeviceHealthMonitor` registers every sensor and actuator independently:
* Monitors continuous heartbeats with timestamp tracking.
* If a sensor ceases transmitting for longer than the configured timeout threshold (**10 seconds**), the monitor declares the device offline and emits a high-priority alarm:
  ```
  [HEALTH-ALERT] 1 device(s) offline/unresponsive:
    -> machine3 / temperature_sensor
  ```

---

### 6. Edge-to-Cloud Integration

#### InfluxDB v2 Time-Series Engine
* **Batch Streaming**: Telemetry from all 3 machines is batched and posted every second via InfluxDB Line Protocol over HTTP.
* **Resilient**: Fault-tolerant design allows uninterrupted factory simulation even if database connectivity drops.
* **Event Store**: Discrete operational events logged to the `events` measurement with severity ratings (`INFO`, `WARNING`, `CRITICAL`).

#### MQTT Telemetry & Remote Command Protocol
* **Telemetry Streaming**: Real-time publishing on `factory/<machineId>/telemetry/<signal>`.
* **Heartbeats**: Published every cycle to `factory/<machineId>/heartbeat`.
* **Command & Control**: Listens on `factory/<machineId>/command` and `factory/all/command`.
  * Supported commands: `STOP`, `SHUTDOWN`, `SET_LOAD <val>`, `SET_FAN <ON|OFF>`, `SET_PUMP <ON|OFF>`, `INJECT_FAULT <TYPE>`, `RESET_FAULT`.
  * Publishes structured JSON execution acknowledgements to `factory/<machineId>/command/ack`.

---

## Project Repository Structure

```
virtual-industrial-iot/
├── .gitignore                      # Git ignore file for CMake and MSBuild
├── CMakeLists.txt                  # Top-level CMake build configuration
├── README.md                       # Comprehensive project documentation
├── config/
│   └── factory.json                # Factory topology, MQTT & InfluxDB config
├── dashboards/
│   ├── industrial_factory_dashboard.json # Cleaned 11-panel Grafana dashboard
│   ├── factory_operating_status.png      # Machine status cards screenshot
│   ├── temperature_automated_cooling.png # Temperature & actuator timelines screenshot
│   ├── operating_telemetry.png           # Load, Current, RPM, Flow screenshot
│   └── scada_event_log.png               # SCADA audit trail table screenshot
├── docs/
│   ├── architecture.md             # Detailed engineering design document
│   ├── SETUP.md                    # Environment and toolchain setup guide
│   └── DEMO.md                     # Step-by-step evaluation guide
├── include/
│   ├── automation/AutomationEngine.h
│   ├── config/ConfigManager.h
│   ├── devices/                    # Sensor and Actuator interface definitions
│   ├── events/                     # Event & EventLogger definitions
│   ├── factory/                    # Machine & Factory domain models
│   ├── health/DeviceHealthMonitor.h
│   ├── mqtt/MQTTClient.h
│   ├── storage/InfluxWriter.h
│   └── telemetry/                  # Telemetry, Validator, & Processor
├── src/
│   ├── main.cpp                    # Factory loop and simulation timeline
│   ├── automation/AutomationEngine.cpp
│   ├── config/ConfigManager.cpp
│   ├── devices/                    # Device implementation classes
│   ├── events/EventLogger.cpp
│   ├── factory/                    # Machine physics & state management
│   ├── health/DeviceHealthMonitor.cpp
│   ├── mqtt/MQTTClient.cpp
│   ├── storage/InfluxWriter.cpp
│   └── telemetry/                  # Telemetry validation & processing logic
└── tests/
    └── test_suite.cpp              # 100% passing automated test suite
```

---

## Quick Start & Build Instructions

### Prerequisites
* **Windows 10/11 x64**
* **CMake 3.20+**
* **Visual Studio 2022 / C++ Build Tools** (MSVC C++17)
* **Eclipse Paho MQTT C++ Library**
* **InfluxDB v2.x** (listening on `localhost:8086`)
* **Mosquitto MQTT Broker** (listening on `localhost:1883`)
* **Grafana** (listening on `localhost:3000`)

### 1. Build the Platform
```powershell
# Generate CMake build files
cmake -B build

# Build Release binaries for platform and tests
cmake --build build --config Release
```

### 2. Run Automated Unit Tests
```powershell
.\build\Release\industrial-iot-tests.exe
```
Output:
```
[TEST 1] Initial machine state: PASSED
[TEST 2] Machine thermal physics: PASSED
[TEST 3] Telemetry validation: PASSED
[TEST 4] Fan automation rule (>= 70 C): PASSED
[TEST 5] Pump automation rule (>= 80 C): PASSED
[TEST 6] Emergency shutdown rule (>= 95 C): PASSED
[TEST 7] Device health monitor & timeouts: PASSED
[TEST 8] Event logger & severity filtering: PASSED
[TEST 9] MQTT command parser: PASSED
All 9 unit tests passed successfully!
```

### 3. Run the Factory Platform Simulation
Ensure InfluxDB and Mosquitto are running, then launch the simulation:
```powershell
.\build\Release\virtual-industrial-iot.exe
```

Credentials, topics, thresholds, and endpoints are automatically loaded from `config/factory.json`.

### 4. View in Grafana
1. Navigate to **`http://localhost:3000`**.
2. Click **Dashboards** $\rightarrow$ **New** $\rightarrow$ **Import**.
3. Upload `dashboards/industrial_factory_dashboard.json`.
4. Select the **InfluxDB** data source and click **Import**.
5. Set the dashboard time range to **Last 15 minutes** (or **Last 1 hour**) and refresh to **5s**.