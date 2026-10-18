# Setup & Deployment Guide

This guide describes how to set up the supporting infrastructure (Mosquitto MQTT, InfluxDB v2, Grafana) and run the Virtual Industrial IoT Platform.

## 1. Mosquitto MQTT Broker

### Installation
- **Windows**: Download from [mosquitto.org/download](https://mosquitto.org/download/) or install via Chocolatey:
  ```powershell
  choco install mosquitto
  ```

### Starting the Broker
Run Mosquitto on port 1883 allowing anonymous connections for local testing:
```powershell
mosquitto -v -p 1883
```

To test MQTT connectivity manually:
```powershell
mosquitto_sub -h 127.0.0.1 -p 1883 -t "factory/#" -v
```

---

## 2. InfluxDB v2 Setup

### Installation & Launch
- Download InfluxDB v2 from [portal.influxdata.com](https://portal.influxdata.com/downloads/).
- Extract and launch:
  ```powershell
  ./influxd.exe
  ```
- By default, InfluxDB serves on `http://localhost:8086`.

### Initial Setup
1. Open your browser at `http://localhost:8086`.
2. Configure initial credentials:
   - **Organization Name**: `athena`
   - **Initial Bucket Name**: `industrial_iot`
   - **User**: `admin`
3. Generate an API token:
   - Navigate to **Data** -> **API Tokens**.
   - Create an All-Access or Custom Token for `industrial_iot`.
   - Copy this token.

### Setting Environment Variables
Set the token in your terminal session before launching the C++ application (credentials are loaded securely from environment variables, never hardcoded in repository files):
```powershell
$env:INFLUX_URL = "http://localhost:8086"
$env:INFLUX_ORG = "athena"
$env:INFLUX_BUCKET = "industrial_iot"
$env:INFLUX_TOKEN = "<paste_your_token_here>"
```
Alternatively, to persist the token across all PowerShell sessions on your machine:
```powershell
[System.Environment]::SetEnvironmentVariable('INFLUX_TOKEN', '<paste_your_token_here>', 'User')
```

---

## 3. Grafana Dashboard Import

### Installation
- Install Grafana via Windows installer or Docker:
  ```powershell
  docker run -d -p 3000:3000 --name grafana grafana/grafana
  ```
- Open `http://localhost:3000` (default login: `admin` / `admin`).

### Adding InfluxDB Data Source
1. In Grafana, navigate to **Connections** -> **Data Sources** -> **Add data source**.
2. Select **InfluxDB**.
3. Set Query Language to **Flux**.
4. Configure connection:
   - **URL**: `http://localhost:8086` (or `http://host.docker.internal:8086` if Grafana is in Docker)
   - Disable **Basic Auth**.
   - Under **InfluxDB Details**:
     - **Organization**: `athena`
     - **Token**: `<your_influx_token>`
     - **Default Bucket**: `industrial_iot`
5. Click **Save & test**.

### Importing the Dashboard
1. Go to **Dashboards** -> **New** -> **Import**.
2. Click **Upload dashboard JSON file** and select:
   `dashboards/industrial_factory_dashboard.json`
3. Select your InfluxDB data source and click **Import**.

---

## 4. Building and Running the Platform

```powershell
cmake -B build
cmake --build build --config Release
.\build\Release\virtual-industrial-iot.exe
```
