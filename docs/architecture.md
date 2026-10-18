# Industrial IoT Platform Architecture

## System Overview
The Virtual Industrial IoT Platform is a modular C++17 software architecture providing end-to-end industrial edge monitoring, rule-based closed-loop automation, time-series storage with write buffering, thread-safe MQTT command handling with broadcast capabilities, and a unified Grafana SCADA visualization dashboard.

---

## Component Hierarchy & Interaction

### 1. Factory & Physical Modeling Layer
- `Factory`: Aggregate container managing registered virtual industrial machines (`Machine A`, `Machine B`, `Machine C`).
- `Machine`: Physics state machine modeling electromechanical and thermodynamic properties:
  - Internal states: `RUNNING` (0), `WARNING` (1), `SHUTDOWN` (2).
  - Actuators: Air cooling fan, liquid coolant pump.
  - Sensors: Temperature, current, RPM, load, coolant flow.
  - Mathematical physics equations:
    ```text
    current = load * 10.0 A
    rpm = load * 30.0 RPM
    coolantFlow = (isPumpRunning() ? 10.0 : 0.0) L/min
    dT/dt = (load * 0.02) - (isFanRunning() ? 1.0 : 0.0) - (isPumpRunning() ? 2.5 : 0.0)
    Shutdown Cooldown: -0.5 °C/s down to ambient floor (25.0 °C)
    ```

### 2. Actuator State Separation
Actuators explicitly separate commanded state from physical operating health:
* **Commanded State** (`isFanCommanded()`, `isPumpCommanded()`): Records whether automation or an operator ordered the actuator ON.
* **Operational Health** (`isFanOperational()`, `isPumpOperational()`): Tracks hardware health (`!fanFailed`, `!pumpFailed`).
* **Running Condition** (`isFanRunning()`, `isPumpRunning()`): True only if commanded ON, hardware operational, and not in shutdown.
* **Cooling Effectiveness** (`getFanCoolingRate()`, `getPumpCoolingRate()`): Direct thermodynamic extraction rate.
* **Telemetry State** (`getFanTelemetryState()`, `getPumpTelemetryState()`):
  - `0`: OFF
  - `1`: ACTIVE / RUNNING
  - `2`: FAILED (hardware failure injected or detected)

### 3. Safety Automation Layer
- `AutomationEngine`: Evaluates physical machine states against dynamic thresholds configured via `ConfigManager`:
  - $T < \text{fanThreshold}$ (70°C): Normal operation (Fan OFF, Pump OFF). Status: `RUNNING`.
  - $T \ge \text{fanThreshold}$ (70°C): Forced-air cooling engaged (Fan ON). Status: `WARNING`.
  - $T \ge \text{pumpThreshold}$ (80°C): Dual cooling engaged (Fan ON, Pump ON). Status: `WARNING`.
  - $T \ge \text{shutdownThreshold}$ (95°C): Emergency shutdown trip. Load, current, RPM cut to 0. Status: `SHUTDOWN`.
- **Fail-Safe Sensor Protection**: If sensor failure occurs, fail-safe cooling activates immediately.
- **Edge-Triggered Event Lifecycle**: Faults and recoveries are logged only on state transitions, eliminating cyclical spam.

### 4. Safe Recovery Rules
- `Machine::attemptRecovery(rejectionReason)`:
  - If machine is shut down and core temperature is $\ge 80^\circ\text{C}$, recovery is rejected to prevent thermal shock and safety hazards.
  - Once cooled below $80^\circ\text{C}$, recovery clears faults, restores shutdown status, and returns load to baseline 60%.

### 5. Health & Availability Layer
- `DeviceHealthMonitor`: Tracks device condition across three distinct dimensions:
  1. `online`: Communication heartbeat received within timeout (10s).
  2. `valid`: Measurement data falls within valid physical ranges.
  3. `operational`: Hardware actuator/motor is free of mechanical faults.
- Emits alarms when devices become degraded, offline, or invalid.

### 6. Telemetry & Validation Layer
- `Telemetry`: Canonical schema containing `machineId`, `deviceId`, `sensorType`, `value`, `quality`, `unit`, and `timestamp`.
- `TelemetryValidator`: Enforces sensor boundaries, valid enumerations, and quality flags. Out-of-band sentinel values (`-999.0°C`) are rejected as genuine sensor readings.
- `TelemetryProcessor`: Sanitizes telemetry data and flags invalid or failing sensor streams.

### 7. Thread-Safe Transport Layer (MQTT)
- `CommandQueue`: Synchronized queue (`std::mutex`, `std::queue`) that decouples asynchronous Paho MQTT callback worker threads from the synchronous simulation loop.
- Subscribes to:
  - `factory/<machineId>/command`: Individual machine control.
  - `factory/all/command`: Broadcast control dispatched across all factory machines.
- Publishes structured JSON acknowledgements to `factory/<machineId>/command/ack` and `factory/all/command/ack`.

### 8. Time-Series Storage Layer (InfluxDB v2)
- `InfluxWriter`: HTTP REST client using native Windows WinHTTP for Line Protocol ingestion.
- Implements **Write Retry Buffering**: Failed telemetry batches and events are buffered and resent upon connection restoration.
- Writes to Org `athena`, Bucket `industrial_iot` at `http://localhost:8086`.

### 9. Visualization Layer (Grafana)
- SCADA dashboard defined in `dashboards/industrial_factory_dashboard.json`.
- Uses templated `${datasource}` variable for complete installation portability.
- Visualizes status cards, thermal curves, actuator timelines (0/1/2), operating gauges, and SCADA audit trail.