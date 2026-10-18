# Industrial IoT Platform Architecture

## System Overview
The Industrial IoT Platform is a modular C++ software architecture providing end-to-end industrial edge monitoring, rule-based automation, time-series storage, and dashboard visualization.

## Component Hierarchy

### 1. Factory & Physical Modeling Layer
- `Factory`: Container managing registered industrial machines.
- `Machine`: State machine simulating physical properties:
  - Internal states: `RUNNING`, `WARNING`, `SHUTDOWN`.
  - Actuators: Air cooling fan, liquid coolant pump.
  - Sensors: Temperature, electrical current, RPM, load, coolant flow.
  - Physics equations:
    ```
    current = load * 10.0
    rpm = load * 30.0
    dT = (0.02 * load - 1.0 * fan - 2.5 * pump) * dt
    ```

### 2. Safety Automation Layer
- `AutomationEngine`: Evaluates machine conditions against deterministic thresholds:
  - Temperature < 70 C: Normal operation (Fan OFF, Pump OFF)
  - Temperature >= 70 C: Fan ON (Air cooling)
  - Temperature >= 80 C: Pump ON, Fan ON (Liquid cooling, Warning state)
  - Temperature >= 95 C: Emergency Shutdown (Equipment protection)

### 3. Fault & Event Management Layer
- `Event`: Structured record containing timestamp, machine ID, event type, severity (`INFO`, `WARNING`, `CRITICAL`), and description.
- `EventLogger`: Centralized event log dispatching events to console, MQTT, and InfluxDB.

### 4. Health & Availability Layer
- `DeviceHealthMonitor`: Tracks sensor and actuator heartbeats.
- Detects unresponsive devices based on heartbeat timeout thresholds.
- Alerts on missing or failing telemetry streams.

### 5. Telemetry & Validation Layer
- `Telemetry`: Unified schema containing `machineId`, `deviceId`, `sensorType`, `value`, `quality`, `unit`, and `timestamp`.
- `TelemetryValidator`: Enforces sensor-specific ranges, non-null values, valid status enums, and data quality flags.
- `TelemetryProcessor`: Sanitizes and validates data points before forwarding to transport layers.

### 6. Edge Transport Layer (MQTT)
- `MQTTClient`: Manages connection, subscriptions, and publication over Eclipse Paho MQTT C++.
- Telemetry topics: `factory/<machineId>/telemetry/<signal>`
- Heartbeat topics: `factory/<machineId>/heartbeat`
- Command topics: `factory/<machineId>/command`
- Command acknowledgement topics: `factory/<machineId>/command/ack`

### 7. Time-Series Storage Layer (InfluxDB v2)
- `InfluxWriter`: HTTP REST client using native Windows WinHTTP for Line Protocol ingestion.
- Batched telemetry point submission:
  ```
  telemetry,machine=machine1,device=temperature_sensor,sensor=temperature,quality=good,unit=C value=74.5 1700000000000000000
  ```
- Event submission:
  ```
  events,machine=machine1,event_type=FAN_ON,severity=INFO message="Fan activated at 70.2 C" 1700000000000000000
  ```

### 8. Visualization Layer (Grafana)
- Provisioned via `dashboards/industrial_factory_dashboard.json`.
- Displays machine status, thermal progression, actuator duty cycles, operating metrics, and fault logs.